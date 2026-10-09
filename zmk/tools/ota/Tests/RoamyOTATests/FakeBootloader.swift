import Foundation
@testable import RoamyOTA

/// A `DFUTransport` that plays the Adafruit bootloader's side of legacy DFU, checks every write,
/// and can be told to fail, stall or drop the link.
final class FakeBootloader: DFUTransport, @unchecked Sendable {
    enum Write: Equatable {
        case control(ControlPointRequest)
        case packet([UInt8])
    }

    struct Faults {
        /// Answer the request with this op code with this status instead of success.
        var status: [OpCode: ResponseCode] = [:]
        /// Never answer the request with this op code.
        var silentOn: OpCode?
        /// Drop the link when this many image packets have arrived.
        var disconnectAfterImagePackets: Int?
        /// Add this to the byte count in every packet receipt.
        var receiptSkew: UInt32 = 0
        /// Drop the link right after answering Validate Firmware.
        var disconnectAfterValidation = false
        /// Reset on Activate and Reset before acknowledging it, as the real bootloader may.
        var resetBeforeAcknowledgingActivate = false
        var revision: DFURevision? = DFURevision(major: 0, minor: 8)
        var maximumPacketLength = 20
    }

    private let lock = NSLock()
    private let notifications = Mailbox<[UInt8]>()
    private let faults: Faults

    // Guarded by `lock`.
    private var log: [Write] = []
    private var problems: [String] = []
    private var state = State.idle
    private var imageSize = 0
    private var initPacket: [UInt8] = []
    private var image: [UInt8] = []
    private var receiptInterval = 0
    private var packetsSinceReceipt = 0
    private var imagePackets = 0
    private var unreadReceipts = 0
    private var readinessChecks = 0
    private var closed = false
    private var receipts: [UInt32] = []

    private enum State { case idle, awaitingSizes, started, receivingInit, initDone, receivingImage, imageDone, validated, activated }

    init(_ faults: Faults = Faults()) {
        self.faults = faults
    }

    var writes: [Write] { lock.withLock { log } }
    var controlRequests: [ControlPointRequest] { writes.compactMap { if case .control(let r) = $0 { r } else { nil } } }
    /// Protocol violations seen so far; empty when the updater behaved.
    var violations: [String] { lock.withLock { problems } }
    var receivedImage: [UInt8] { lock.withLock { image } }
    var receivedInitPacket: [UInt8] { lock.withLock { initPacket } }
    var readinessCheckCount: Int { lock.withLock { readinessChecks } }
    var imagePacketCount: Int { lock.withLock { imagePackets } }
    /// The byte counts of the packet receipts sent, in order.
    var receiptsSent: [UInt32] { lock.withLock { receipts } }

    // MARK: DFUTransport

    func readRevision() async throws -> DFURevision? {
        try check()
        return faults.revision
    }

    func writeControlPoint(_ value: [UInt8]) async throws {
        try check()
        lock.withLock { handleControl(value) }
        let request = ControlPointRequest(bytes: value)
        if request == .validateFirmware, faults.disconnectAfterValidation { disconnect() }
        if request == .activateAndReset, faults.resetBeforeAcknowledgingActivate {
            disconnect()
            throw DFUTransportError.droppedBeforeAcknowledgement("link lost")
        }
    }

    private func handleControl(_ value: [UInt8]) {
        guard let request = ControlPointRequest(bytes: value) else {
            problems.append("unknown control point write \(value)")
            return
        }
        log.append(.control(request))
        switch (request, state) {
        case (.startDFU(.application), .idle):
            state = .awaitingSizes
        case (.initPacketBegin, .started):
            state = .receivingInit
        case (.initPacketComplete, .receivingInit):
            state = .initDone
            respond(.initDFUParameters)
        case (.packetReceiptNotificationRequest(let n), .initDone):
            receiptInterval = Int(n)
            packetsSinceReceipt = 0
        case (.receiveFirmwareImage, .initDone):
            state = .receivingImage
        case (.validateFirmware, .imageDone):
            state = .validated
            let crc = InitPacket(bytesOrNil: initPacket)?.imageCRC16
            respond(.validateFirmware, status: crc == crc16(image) ? .success : .crcError)
        case (.activateAndReset, .validated):
            state = .activated
        case (.reset, _):
            state = .idle
        default:
            problems.append("\(request) in state \(state)")
        }
    }

    func waitUntilReadyToSendPacket() async throws {
        try check()
        await Task.yield()
        lock.withLock { readinessChecks += 1 }
    }

    func writePacket(_ value: [UInt8]) async throws {
        try check()
        if lock.withLock({ handlePacket(value) }) { disconnect() }
    }

    /// - Returns: Whether the link should drop now.
    private func handlePacket(_ value: [UInt8]) -> Bool {
        log.append(.packet(value))
        switch state {
        case .awaitingSizes:
            guard let sizes = ImageSizes(bytes: value) else {
                problems.append("image sizes of \(value.count) bytes")
                break
            }
            imageSize = Int(sizes.application)
            state = .started
            respond(.startDFU)
        case .receivingInit:
            initPacket += value
        case .receivingImage:
            if unreadReceipts > 0 { problems.append("image packet sent before the previous receipt was read") }
            if value.count > faults.maximumPacketLength || value.count % 4 != 0 { problems.append("image packet of \(value.count) bytes") }
            image += value
            imagePackets += 1
            packetsSinceReceipt += 1
            if image.count >= imageSize {
                state = .imageDone
                respond(.receiveFirmwareImage)
            } else if receiptInterval > 0, packetsSinceReceipt == receiptInterval {
                packetsSinceReceipt = 0
                unreadReceipts += 1
                let receipt = ControlPointNotification.packetReceipt(bytesReceived: UInt32(image.count) + faults.receiptSkew)
                receipts.append(UInt32(image.count) + faults.receiptSkew)
                notifications.post(receipt.bytes)
            }
            if imagePackets == faults.disconnectAfterImagePackets { return true }
        default:
            problems.append("packet of \(value.count) bytes in state \(state)")
        }
        return false
    }

    func nextNotification() async throws -> [UInt8] {
        let bytes = try await notifications.receive()
        if ControlPointNotification(bytes: bytes).map({ if case .packetReceipt = $0 { true } else { false } }) == true {
            lock.withLock { unreadReceipts -= 1 }
        }
        return bytes
    }

    func maximumPacketLength() async -> Int { faults.maximumPacketLength }

    // MARK: Faults

    func disconnect() {
        lock.withLock { closed = true }
        notifications.close(throwing: DFUTransportError.disconnected("link lost"))
    }

    /// With `lock` held.
    private func respond(_ request: OpCode, status: ResponseCode = .success) {
        guard faults.silentOn != request else { return }
        let status = faults.status[request] ?? status
        notifications.post(ControlPointNotification.response(request: request, status: status).bytes)
    }

    private func check() throws {
        if lock.withLock({ closed }) { throw DFUTransportError.disconnected("link lost") }
    }
}

extension InitPacket {
    init?(bytesOrNil bytes: [UInt8]) {
        guard let packet = try? InitPacket(bytes: bytes) else { return nil }
        self = packet
    }
}
