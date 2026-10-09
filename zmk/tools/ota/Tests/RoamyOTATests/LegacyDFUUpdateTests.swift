import Foundation
import Testing
@testable import RoamyOTA

@Suite(.timeLimit(.minutes(1)))
struct LegacyDFUUpdateTests {
    private final class EventLog: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [DFUEvent] = []
        func append(_ event: DFUEvent) { lock.withLock { stored.append(event) } }
        var events: [DFUEvent] { lock.withLock { stored } }
    }

    private func run(_ bootloader: FakeBootloader, imageSize: Int = 1000, settings: DFUSettings = DFUSettings(), events: EventLog = EventLog()) async throws -> DFUFailure? {
        let update = try LegacyDFUUpdate(zip: Fixtures.dfuZip(imageSize: imageSize), settings: settings)
        do {
            try await update.run(over: bootloader) { events.append($0) }
            return nil
        } catch {
            return error
        }
    }

    @Test func happyPath() async throws {
        let bootloader = FakeBootloader()
        let events = EventLog()
        let zip = try Fixtures.dfuZip(imageSize: 1000)

        let failure = try await run(bootloader, events: events)

        #expect(failure == nil)
        #expect(bootloader.violations == [])
        #expect(bootloader.controlRequests == [
            .startDFU(.application), .initPacketBegin, .initPacketComplete,
            .packetReceiptNotificationRequest(8), .receiveFirmwareImage, .validateFirmware, .activateAndReset,
        ])
        #expect(Array(bootloader.writes.prefix(5)) == [
            .control(.startDFU(.application)),
            .packet(ImageSizes(application: 1000).bytes),
            .control(.initPacketBegin),
            .packet(zip.initPacket.bytes),
            .control(.initPacketComplete),
        ])
        #expect(bootloader.receivedImage == zip.image)
        #expect(bootloader.receivedInitPacket == zip.initPacket.bytes)
        #expect(bootloader.readinessCheckCount == bootloader.writes.filter { if case .packet = $0 { true } else { false } }.count)
        #expect(events.events.compactMap { (event: DFUEvent) -> DFUPhase? in if case .phase(let p) = event { p } else { nil } } == [
            .checkBootloader, .start, .initPacket, .packetReceipts, .upload, .validate, .activate,
        ])
        #expect(events.events.last { if case .uploaded = $0 { true } else { false } } == .uploaded(sent: 1000, total: 1000))
    }

    @Test(arguments: [(1000, 8), (960, 8), (1004, 4), (40, 1), (20, 8)])
    func receiptEveryIntervalExceptAfterTheLastPacket(imageSize: Int, interval: Int) async throws {
        let bootloader = FakeBootloader()
        var settings = DFUSettings()
        settings.packetReceiptInterval = try PacketReceiptInterval(interval)

        let failure = try await run(bootloader, imageSize: imageSize, settings: settings)

        #expect(failure == nil)
        #expect(bootloader.violations == [])
        let packets = (imageSize + 19) / 20
        let expected = stride(from: interval, to: packets, by: interval).map { UInt32($0 * 20) }
        #expect(bootloader.receiptsSent == expected)
        #expect(bootloader.imagePacketCount == packets)
    }

    @Test func receiptWithWrongByteCountFailsTheUpload() async throws {
        var faults = FakeBootloader.Faults()
        faults.receiptSkew = 4
        let bootloader = FakeBootloader(faults)

        let failure = try await run(bootloader)

        #expect(failure == DFUFailure(phase: .upload, reason: .receiptMismatch(sent: 160, received: 164)))
        #expect(bootloader.controlRequests.last == .reset)
    }

    @Test(arguments: [
        (OpCode.startDFU, ResponseCode.dataSizeExceedsLimit, DFUPhase.start),
        (.initDFUParameters, .operationFailed, .initPacket),
        (.receiveFirmwareImage, .operationFailed, .upload),
        (.validateFirmware, .crcError, .validate),
    ])
    func errorResponseFailsItsPhase(request: OpCode, status: ResponseCode, phase: DFUPhase) async throws {
        var faults = FakeBootloader.Faults()
        faults.status[request] = status
        let bootloader = FakeBootloader(faults)

        let failure = try #require(try await run(bootloader))

        #expect(failure == DFUFailure(phase: phase, reason: .bootloaderResponse(status)))
        #expect(failure.description.contains("the bootloader answered \(status)"))
        // After Start DFU the transfer is reset so that the next attempt starts clean.
        #expect((bootloader.controlRequests.last == .reset) == (phase != .start))
        #expect(!bootloader.controlRequests.contains(.activateAndReset))
    }

    @Test func interruptedEarlierTransferIsReset() async throws {
        var faults = FakeBootloader.Faults()
        faults.status[.startDFU] = .invalidState
        let bootloader = FakeBootloader(faults)

        let failure = try #require(try await run(bootloader))

        #expect(failure == DFUFailure(phase: .start, reason: .interruptedTransferReset))
        #expect(failure.firmwareErased)
        #expect(bootloader.controlRequests == [.startDFU(.application), .reset])
    }

    @Test(arguments: [DFURevision(major: 0, minor: 1), nil])
    func refusesAnythingButTheBootloader(revision: DFURevision?) async throws {
        var faults = FakeBootloader.Faults()
        faults.revision = revision
        let bootloader = FakeBootloader(faults)

        let failure = try await run(bootloader)

        #expect(failure == DFUFailure(phase: .checkBootloader, reason: .notABootloader(revision)))
        #expect(failure?.firmwareErased == false)
        #expect(bootloader.writes == [])
    }

    @Test func disconnectMidTransferFailsTheUpload() async throws {
        var faults = FakeBootloader.Faults()
        faults.disconnectAfterImagePackets = 20
        let bootloader = FakeBootloader(faults)

        let failure = try await run(bootloader)

        #expect(failure == DFUFailure(phase: .upload, reason: .transport(.disconnected("link lost"))))
        #expect(failure?.description.contains("uploading failed: the connection dropped (link lost)") == true)
        #expect(!bootloader.controlRequests.contains(.reset))
    }

    @Test func activationCountsWhenTheBootloaderResetsBeforeAcknowledging() async throws {
        var faults = FakeBootloader.Faults()
        faults.resetBeforeAcknowledgingActivate = true
        let bootloader = FakeBootloader(faults)

        #expect(try await run(bootloader) == nil)
        #expect(bootloader.controlRequests.last == .activateAndReset)
    }

    @Test func linkLostBeforeActivationFails() async throws {
        var faults = FakeBootloader.Faults()
        faults.disconnectAfterValidation = true
        let bootloader = FakeBootloader(faults)

        let failure = try await run(bootloader)

        #expect(failure == DFUFailure(phase: .activate, reason: .transport(.disconnected("link lost"))))
        #expect(!bootloader.controlRequests.contains(.activateAndReset))
    }

    @Test func silentBootloaderTimesOut() async throws {
        var faults = FakeBootloader.Faults()
        faults.silentOn = .validateFirmware
        let bootloader = FakeBootloader(faults)
        var settings = DFUSettings()
        settings.timeouts.response = .milliseconds(100)

        let failure = try await run(bootloader, settings: settings)

        #expect(failure == DFUFailure(phase: .validate, reason: .timedOut(.milliseconds(100))))
        #expect(bootloader.controlRequests.last == .reset)
    }

    @Test func cancellationEndsAWaitingUpdate() async throws {
        var faults = FakeBootloader.Faults()
        faults.silentOn = .validateFirmware
        let bootloader = FakeBootloader(faults)
        let zip = try Fixtures.dfuZip()

        let task = Task { () throws -> DFUFailure? in
            do {
                try await LegacyDFUUpdate(zip: zip, settings: DFUSettings()).run(over: bootloader)
                return nil
            } catch let error as DFUFailure {
                return error
            }
        }
        while !bootloader.controlRequests.contains(.validateFirmware) {
            try await Task.sleep(for: .milliseconds(5))
        }
        task.cancel()

        #expect(try await task.value == DFUFailure(phase: .validate, reason: .cancelled))
        #expect(!bootloader.controlRequests.contains(.reset))
    }

    @Test func packetLargerThanTheLinkIsRefused() async throws {
        let bootloader = FakeBootloader()
        var settings = DFUSettings()
        settings.packetSize = 24

        let failure = try await run(bootloader, settings: settings)

        #expect(failure == DFUFailure(phase: .upload, reason: .packetSizeUnsupported(requested: 24, maximum: 20)))
    }

    @Test func largerPacketsWhenTheLinkTakesThem() async throws {
        var faults = FakeBootloader.Faults()
        faults.maximumPacketLength = 244
        let bootloader = FakeBootloader(faults)
        var settings = DFUSettings()
        settings.packetSize = 244

        let failure = try await run(bootloader, imageSize: 4000, settings: settings)

        #expect(failure == nil)
        #expect(bootloader.violations == [])
        #expect(bootloader.receiptsSent == [1952, 3904])
    }
}
