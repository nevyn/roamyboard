import Foundation

/// How many image packets the bootloader takes before it sends a packet receipt. Each receipt
/// pauses the upload until it arrives, which keeps the bootloader's receive buffers from
/// overflowing: Adafruit's README requires 8 or less for OTA.
public struct PacketReceiptInterval: Equatable, Sendable, CustomStringConvertible {
    public let packets: UInt16

    public static let allowed: ClosedRange<Int> = 1...8
    public static let `default` = PacketReceiptInterval(uncheckedPackets: 8)

    /// - Throws: `OptionError.invalidValue` unless `packets` lies in `allowed`.
    public init(_ packets: Int) throws(OptionError) {
        guard Self.allowed.contains(packets) else {
            throw .invalidValue(option: "--prn", value: String(packets), reason: "the bootloader runs out of memory above 8 packets per receipt, and 0 turns receipts off; use 1 to 8")
        }
        self.packets = UInt16(packets)
    }

    private init(uncheckedPackets: UInt16) { packets = uncheckedPackets }

    public var description: String { String(packets) }
}

/// How long `LegacyDFUUpdate` waits for each step before it fails the half.
public struct DFUTimeouts: Sendable {
    /// A control point write acknowledgement, a revision read, a packet receipt, or readiness
    /// to send the next packet.
    public var step: Duration = .seconds(10)
    /// The response to Start DFU, which comes after the bootloader has erased the old image.
    public var erase: Duration = .seconds(60)
    /// The responses to the init packet, the last image packet and Validate Firmware.
    public var response: Duration = .seconds(30)

    public init() {}
}

/// Parameters of one update.
public struct DFUSettings: Sendable {
    public var packetReceiptInterval: PacketReceiptInterval = .default
    /// Bytes per write to the packet characteristic. The bootloader takes whole 4-byte words;
    /// Nordic's legacy DFU uses 20, which fits the smallest ATT MTU.
    public var packetSize = 20
    public var timeouts = DFUTimeouts()

    public init() {}
}

/// A stage of the legacy DFU sequence, named in progress and failures.
public enum DFUPhase: String, Sendable, CustomStringConvertible {
    case checkBootloader
    case start
    case initPacket
    case packetReceipts
    case upload
    case validate
    case activate

    public var description: String {
        switch self {
        case .checkBootloader: "checking the bootloader"
        case .start: "starting (the bootloader erases the old firmware)"
        case .initPacket: "sending the init packet"
        case .packetReceipts: "requesting packet receipts"
        case .upload: "uploading"
        case .validate: "validating"
        case .activate: "activating"
        }
    }
}

/// Progress reported by `LegacyDFUUpdate.run`.
public enum DFUEvent: Equatable, Sendable {
    case phase(DFUPhase)
    /// `sent` of `total` image bytes are queued for the bootloader.
    case uploaded(sent: Int, total: Int)
}

/// Why a half's update failed, and in which phase.
public struct DFUFailure: Error, Equatable, CustomStringConvertible {
    public let phase: DFUPhase
    public let reason: Reason

    public enum Reason: Error, Equatable, Sendable {
        /// The bootloader answered the phase's request with this code.
        case bootloaderResponse(ResponseCode)
        case transport(DFUTransportError)
        case timedOut(Duration)
        /// A packet receipt reported a byte count other than the bytes sent.
        case receiptMismatch(sent: UInt32, received: UInt32)
        /// A notification arrived that the phase does not expect; the bytes are kept.
        case unexpectedNotification([UInt8])
        /// The DFU service is not a bootloader's: no revision, or the application-mode 0.1.
        case notABootloader(DFURevision?)
        /// `DFUSettings.packetSize` is not a positive multiple of 4 or exceeds the link's maximum.
        case packetSizeUnsupported(requested: Int, maximum: Int)
        /// Start DFU found an interrupted earlier transfer; the bootloader was told to reset.
        case interruptedTransferReset
        case cancelled
    }

    public init(phase: DFUPhase, reason: Reason) {
        self.phase = phase
        self.reason = reason
    }

    /// Whether the half has lost its old firmware: true once Start DFU went through.
    public var firmwareErased: Bool {
        switch phase {
        case .checkBootloader: false
        case .start: reason == .interruptedTransferReset
        default: true
        }
    }

    public var description: String {
        let what: String = switch reason {
        case .bootloaderResponse(let code): "the bootloader answered \(code)"
        case .transport(let error): "\(error)"
        case .timedOut(let duration): "no answer within \(duration.formatted(.units(allowed: [.seconds])))"
        case .receiptMismatch(let sent, let received): "the bootloader confirmed \(received) bytes after \(sent) were sent"
        case .unexpectedNotification(let bytes): "unexpected notification \(bytes.map { String(format: "%02X", $0) }.joined(separator: " "))"
        case .notABootloader(let revision?): "the DFU service reports revision \(revision), which is an application, not the bootloader"
        case .notABootloader(nil): "the DFU service has no revision characteristic, so it is not the Adafruit bootloader"
        case .packetSizeUnsupported(let requested, let maximum): "packet size \(requested) does not fit: it must be a multiple of 4 and at most \(maximum) on this connection"
        case .interruptedTransferReset: "the bootloader still held an interrupted transfer, so roamy-ota reset it"
        case .cancelled: "cancelled"
        }
        let advice = advice.map { " \($0)" } ?? ""
        return "\(phase) failed: \(what).\(advice)"
    }

    private var advice: String? {
        switch (phase, reason) {
        case (.start, .interruptedTransferReset):
            "Wait until the half advertises as AdaDFU again, then run roamy-ota again for it."
        case (.start, .bootloaderResponse(.dataSizeExceedsLimit)):
            "The image is larger than the bootloader's application area."
        case (.initPacket, .bootloaderResponse(.operationFailed)):
            "The bootloader rejected the init packet; make the DFU zip with --dev-type 0x0052 --sd-req 0xFFFE."
        case (.upload, .bootloaderResponse(.operationFailed)), (.upload, .receiptMismatch):
            "The bootloader lost image data; try a lower --prn."
        case (.validate, .bootloaderResponse(.crcError)):
            "The image arrived corrupted."
        default:
            nil
        }
    }
}

/// Sends a DFU zip's application image to the Adafruit nRF52 bootloader in OTA mode, with
/// Nordic's legacy DFU sequence: Start DFU and the image sizes, the init packet, packet receipts,
/// the image, Validate Firmware, Activate and Reset. See docs/firmware.md.
public struct LegacyDFUUpdate: Sendable {
    public let zip: DFUZip
    public let settings: DFUSettings

    public init(zip: DFUZip, settings: DFUSettings) {
        self.zip = zip
        self.settings = settings
    }

    /// Runs the whole sequence over `transport`.
    ///
    /// Returns once the bootloader validated the image and took Activate and Reset; the half then
    /// reboots into the new firmware and drops the link. If a failure leaves the link up after
    /// Start DFU, it asks the bootloader to reset, so that the next attempt starts clean.
    /// - Parameter events: Called with each phase and after each image packet.
    /// - Throws: `DFUFailure` naming the phase and the reason; `.cancelled` if the task is cancelled.
    public func run(over transport: some DFUTransport, events: @Sendable (DFUEvent) -> Void = { _ in }) async throws(DFUFailure) {
        var started = false
        do {
            try await steps(transport, events: events, started: &started)
        } catch {
            if started, error.reason.leavesLinkUp {
                _ = try? await withTimeout(.seconds(2)) { try await transport.writeControlPoint(ControlPointRequest.reset.bytes) }
            }
            throw error
        }
    }

    private func steps(_ transport: some DFUTransport, events: @Sendable (DFUEvent) -> Void, started: inout Bool) async throws(DFUFailure) {
        let link = Link(transport: transport, timeouts: settings.timeouts)

        events(.phase(.checkBootloader))
        try await phase(.checkBootloader) { () async throws(DFUFailure.Reason) in
            let revision = try await link.timed(link.timeouts.step) { try await transport.readRevision() }
            guard let revision, !revision.isApplicationMode else { throw DFUFailure.Reason.notABootloader(revision) }
        }

        events(.phase(.start))
        try await phase(.start) { () async throws(DFUFailure.Reason) in
            try await link.control(.startDFU(zip.imageType))
            try await link.packet(zip.imageSizes.bytes)
            let status = try await link.response(to: .startDFU, within: link.timeouts.erase)
            if status == .invalidState {
                _ = try? await link.control(.reset)
                throw DFUFailure.Reason.interruptedTransferReset
            }
            try check(status)
        }
        started = true

        events(.phase(.initPacket))
        try await phase(.initPacket) { () async throws(DFUFailure.Reason) in
            try await link.control(.initPacketBegin)
            for chunk in zip.initPacket.bytes.chunks(of: settings.packetSize) {
                try await link.packet(chunk)
            }
            try await link.control(.initPacketComplete)
            try check(await link.response(to: .initDFUParameters, within: link.timeouts.response))
        }

        events(.phase(.packetReceipts))
        try await phase(.packetReceipts) { () async throws(DFUFailure.Reason) in
            try await link.control(.packetReceiptNotificationRequest(settings.packetReceiptInterval.packets))
        }

        events(.phase(.upload))
        try await phase(.upload) { () async throws(DFUFailure.Reason) in
            let maximum = await transport.maximumPacketLength()
            let size = settings.packetSize
            guard size > 0, size % 4 == 0, size <= maximum else {
                throw DFUFailure.Reason.packetSizeUnsupported(requested: size, maximum: maximum)
            }
            try await link.control(.receiveFirmwareImage)
            let total = zip.image.count
            let packets = zip.image.chunks(of: size)
            let interval = Int(settings.packetReceiptInterval.packets)
            var sent = 0
            for (index, packet) in packets.enumerated() {
                try await link.packet(packet)
                sent += packet.count
                events(.uploaded(sent: sent, total: total))
                // The bootloader sends no receipt for the last packet, only the final response.
                guard (index + 1) % interval == 0, sent < total else { continue }
                let notification = try await link.notification(within: link.timeouts.step)
                switch notification {
                case .packetReceipt(let received) where received == UInt32(sent):
                    continue
                case .packetReceipt(let received):
                    throw DFUFailure.Reason.receiptMismatch(sent: UInt32(sent), received: received)
                case .response(.receiveFirmwareImage, let status) where status != .success:
                    throw DFUFailure.Reason.bootloaderResponse(status)
                default:
                    throw DFUFailure.Reason.unexpectedNotification(notification.bytes)
                }
            }
            try check(await link.response(to: .receiveFirmwareImage, within: link.timeouts.response))
        }

        events(.phase(.validate))
        try await phase(.validate) { () async throws(DFUFailure.Reason) in
            try await link.control(.validateFirmware)
            try check(await link.response(to: .validateFirmware, within: link.timeouts.response))
        }

        events(.phase(.activate))
        try await phase(.activate) { () async throws(DFUFailure.Reason) in
            do throws(DFUFailure.Reason) {
                try await link.control(.activateAndReset)
            } catch .transport(.droppedBeforeAcknowledgement), .transport(.writeFailed) {
                // The bootloader may reset before it acknowledges; the write went out either way.
            }
        }
    }

    private func check(_ status: ResponseCode) throws(DFUFailure.Reason) {
        guard status == .success else { throw .bootloaderResponse(status) }
    }

    private func phase(_ phase: DFUPhase, _ body: () async throws(DFUFailure.Reason) -> Void) async throws(DFUFailure) {
        do {
            try await body()
        } catch {
            throw DFUFailure(phase: phase, reason: error)
        }
    }
}

/// The transport with the protocol's request/response shapes and timeouts; it throws only
/// `DFUFailure.Reason`.
private struct Link<Transport: DFUTransport>: Sendable {
    let transport: Transport
    let timeouts: DFUTimeouts

    func timed<T: Sendable>(_ duration: Duration, _ operation: @escaping @Sendable () async throws -> T) async throws(DFUFailure.Reason) -> T {
        do {
            return try await withTimeout(duration, operation)
        } catch let error as TimeoutError {
            throw .timedOut(error.duration)
        } catch let error as DFUTransportError {
            throw .transport(error)
        } catch is CancellationError {
            throw .cancelled
        } catch {
            throw .transport(.failed(String(describing: error)))
        }
    }

    func control(_ request: ControlPointRequest) async throws(DFUFailure.Reason) {
        try await timed(timeouts.step) { [transport] in try await transport.writeControlPoint(request.bytes) }
    }

    func packet(_ bytes: [UInt8]) async throws(DFUFailure.Reason) {
        try await timed(timeouts.step) { [transport] in
            try await transport.waitUntilReadyToSendPacket()
            try await transport.writePacket(bytes)
        }
    }

    func notification(within duration: Duration) async throws(DFUFailure.Reason) -> ControlPointNotification {
        let bytes = try await timed(duration) { [transport] in try await transport.nextNotification() }
        guard let notification = ControlPointNotification(bytes: bytes) else { throw .unexpectedNotification(bytes) }
        return notification
    }

    func response(to request: OpCode, within duration: Duration) async throws(DFUFailure.Reason) -> ResponseCode {
        let notification = try await notification(within: duration)
        guard case .response(request, let status) = notification else { throw .unexpectedNotification(notification.bytes) }
        return status
    }
}

extension DFUFailure.Reason {
    fileprivate var leavesLinkUp: Bool {
        switch self {
        case .transport(.disconnected), .transport(.droppedBeforeAcknowledgement), .cancelled, .interruptedTransferReset: false
        default: true
        }
    }
}

extension Array {
    func chunks(of size: Int) -> [[Element]] {
        stride(from: 0, to: count, by: size).map { Array(self[$0..<Swift.min($0 + size, count)]) }
    }
}
