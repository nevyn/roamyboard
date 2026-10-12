import Foundation
import Testing
@testable import RoamyOTA

@Suite(.timeLimit(.minutes(1)))
struct HalfUpdateTests {
    private final class EventLog: @unchecked Sendable {
        private let lock = NSLock()
        private var stored: [HalfUpdateEvent] = []
        func append(_ event: HalfUpdateEvent) { lock.withLock { stored.append(event) } }
        /// Every event but DFU progress.
        var steps: [HalfUpdateEvent] { lock.withLock { stored.filter { if case .dfu = $0 { false } else { true } } } }
    }

    private static let dropMidUpload = {
        var faults = FakeBootloader.Faults()
        faults.disconnectAfterImagePackets = 20
        return faults
    }()

    private static let interruptedTransfer = {
        var faults = FakeBootloader.Faults()
        faults.interruptedTransfer = true
        return faults
    }()

    private static let dropped = HalfUpdateFailure.Reason.dfu(DFUFailure(phase: .upload, reason: .transport(.disconnected("link lost"))))

    private func run(_ peripheral: FakePeripheral, policy: RetryPolicy = RetryPolicy(), events: EventLog = EventLog()) async throws -> HalfUpdateFailure? {
        let update = try HalfUpdate(update: LegacyDFUUpdate(zip: Fixtures.dfuZip(), settings: DFUSettings()), policy: policy)
        do {
            try await update.run(on: peripheral) { events.append($0) }
            return nil
        } catch {
            return error
        }
    }

    @Test func dropMidUploadIsRetriedAfterResettingTheInterruptedTransfer() async throws {
        let peripheral = FakePeripheral(connections: [Self.dropMidUpload, FakeBootloader.Faults()])
        let events = EventLog()

        let failure = try await run(peripheral, events: events)

        #expect(failure == nil)
        #expect(events.steps == [
            .connecting(attempt: 1),
            .retrying(attempt: 2, limit: 3, reason: Self.dropped),
            .waitingForAdvertisement, .connecting(attempt: 2),
            .resetInterruptedTransfer(attempt: 2),
            .waitingForAdvertisement, .connecting(attempt: 2),
        ])
        let bootloaders = peripheral.bootloaders
        try #require(bootloaders.count == 3)
        #expect(!bootloaders[0].controlRequests.contains(.reset))
        #expect(bootloaders[1].controlRequests == [.startDFU(.application), .reset])
        #expect(bootloaders[2].receivedImage == Fixtures.image(size: 1000))
        #expect(bootloaders[2].controlRequests.last == .activateAndReset)
        #expect(bootloaders.flatMap(\.violations) == [])
    }

    @Test func interruptedTransferOnTheFirstAttemptIsResetAndRunAgain() async throws {
        let peripheral = FakePeripheral(connections: [Self.interruptedTransfer, FakeBootloader.Faults()])
        let events = EventLog()

        let failure = try await run(peripheral, events: events)

        #expect(failure == nil)
        #expect(events.steps == [
            .connecting(attempt: 1), .resetInterruptedTransfer(attempt: 1), .waitingForAdvertisement, .connecting(attempt: 1),
        ])
        #expect(peripheral.bootloaders.map(\.controlRequests.last) == [.reset, .activateAndReset])
    }

    @Test func givesUpWhenTheRetriesRunOut() async throws {
        let peripheral = FakePeripheral(connections: [Self.dropMidUpload])
        let events = EventLog()

        let failure = try #require(try await run(peripheral, events: events))

        #expect(failure.reason == Self.dropped)
        #expect(failure.attempts == 3)
        #expect(failure.firmwareErased)
        #expect(events.steps.filter { if case .retrying = $0 { true } else { false } }.count == 2)
        // Each retry first resets the transfer that the previous attempt left.
        #expect(peripheral.bootloaders.count == 5)
        #expect(failure.description.contains("gave up after 3 attempts"))
        #expect(failure.description.contains("run roamy-ota again with roamyboard_left.zip"))
        #expect(failure.description.contains("flash the UF2 over USB"))
    }

    @Test(arguments: [dropMidUpload, interruptedTransfer])
    func zeroRetriesNeverReconnects(faults: FakeBootloader.Faults) async throws {
        let peripheral = FakePeripheral(connections: [faults])
        var policy = RetryPolicy()
        policy.retries = 0

        let failure = try #require(try await run(peripheral, policy: policy))

        #expect(failure.attempts == 1)
        #expect(failure.firmwareErased)
        #expect(peripheral.connectionAttempts == 1)
        #expect(peripheral.advertisementWaits == 0)
        if faults.interruptedTransfer {
            #expect(failure.reason == .dfu(DFUFailure(phase: .start, reason: .interruptedTransferReset)))
        } else {
            #expect(failure.reason == Self.dropped)
        }
    }

    @Test(arguments: [
        (OpCode.validateFirmware, ResponseCode.crcError, DFUPhase.validate),
        (.startDFU, .dataSizeExceedsLimit, .start),
        (.initDFUParameters, .operationFailed, .initPacket),
    ])
    func errorResponseFailsAtOnce(request: OpCode, status: ResponseCode, phase: DFUPhase) async throws {
        var faults = FakeBootloader.Faults()
        faults.status[request] = status
        let peripheral = FakePeripheral(connections: [faults])

        let failure = try #require(try await run(peripheral))

        #expect(failure.reason == .dfu(DFUFailure(phase: phase, reason: .bootloaderResponse(status))))
        #expect(failure.attempts == 1)
        #expect(peripheral.connectionAttempts == 1)
        #expect(peripheral.advertisementWaits == 0)
    }

    @Test func receiptMismatchFailsAtOnce() async throws {
        var faults = FakeBootloader.Faults()
        faults.receiptSkew = 4
        let peripheral = FakePeripheral(connections: [faults])

        let failure = try #require(try await run(peripheral))

        #expect(failure.reason == .dfu(DFUFailure(phase: .upload, reason: .receiptMismatch(sent: 160, received: 164))))
        #expect(peripheral.connectionAttempts == 1)
    }

    @Test func failedConnectionIsRetried() async throws {
        let peripheral = FakePeripheral(failingConnections: [0])
        let events = EventLog()

        let failure = try await run(peripheral, events: events)

        #expect(failure == nil)
        #expect(events.steps == [
            .connecting(attempt: 1),
            .retrying(attempt: 2, limit: 3, reason: .connect("the connection dropped (no route)")),
            .waitingForAdvertisement, .connecting(attempt: 2),
        ])
    }

    @Test func halfThatDoesNotAdvertiseAgainFailsWithAdvice() async throws {
        let erasedHalf = FakePeripheral(connections: [Self.dropMidUpload], advertisesAgain: false)
        let intactHalf = FakePeripheral(failingConnections: [0], advertisesAgain: false)
        var policy = RetryPolicy()
        policy.readvertiseTimeout = .milliseconds(100)

        let erased = try #require(try await run(erasedHalf, policy: policy))
        let intact = try #require(try await run(intactHalf, policy: policy))

        #expect(erased.reason == .notAdvertising(.milliseconds(100)))
        #expect(erased.attempts == 2)
        #expect(erased.firmwareErased)
        #expect(erased.description.contains("has no firmware now: double-tap its reset button and flash the UF2 over USB"))
        #expect(intact.reason == .notAdvertising(.milliseconds(100)))
        #expect(!intact.firmwareErased)
        #expect(intact.description.contains("Press OTA on that half and run roamy-ota again with roamyboard_left.zip"))
    }

    @Test func cancellationWhileWaitingForTheHalfEndsTheUpdate() async throws {
        let peripheral = FakePeripheral(connections: [Self.dropMidUpload], advertisesAgain: false)
        let update = try HalfUpdate(update: LegacyDFUUpdate(zip: Fixtures.dfuZip(), settings: DFUSettings()), policy: RetryPolicy())

        let task = Task { () -> HalfUpdateFailure? in
            do throws(HalfUpdateFailure) {
                try await update.run(on: peripheral)
                return nil
            } catch {
                return error
            }
        }
        while !peripheral.isWaitingForAdvertisement {
            try await Task.sleep(for: .milliseconds(5))
        }
        task.cancel()

        let failure = try #require(await task.value as HalfUpdateFailure?)
        #expect(failure.reason == .cancelled)
        #expect(failure.firmwareErased)
        #expect(!peripheral.isWaitingForAdvertisement)
    }
}
