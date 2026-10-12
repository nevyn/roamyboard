import Foundation
@testable import RoamyOTA

/// A `DFUPeripheral` that hands out a new `FakeBootloader` for each connection, and carries an
/// interrupted transfer from one connection to the next, as the half's bootloader does.
final class FakePeripheral: DFUPeripheral, @unchecked Sendable {
    private let lock = NSLock()
    private let script: [FakeBootloader.Faults]
    private let failingConnections: Set<Int>
    private let advertisesAgain: Bool

    // Guarded by `lock`.
    private var connections = 0
    private var made: [FakeBootloader] = []
    private var holdsTransfer = false
    private var waits = 0
    private var waiting = false

    /// - Parameters:
    ///   - connections: The faults of each connection's bootloader, in order; the last one repeats.
    ///   - failingConnections: Indices of connection attempts, from 0, that fail with
    ///     `DFUTransportError.disconnected` before a bootloader is made.
    ///   - advertisesAgain: Whether `waitForAdvertisement` returns; if not, it waits until cancelled.
    init(connections: [FakeBootloader.Faults] = [FakeBootloader.Faults()], failingConnections: Set<Int> = [], advertisesAgain: Bool = true) {
        script = connections
        self.failingConnections = failingConnections
        self.advertisesAgain = advertisesAgain
    }

    var bootloaders: [FakeBootloader] { lock.withLock { made } }
    var connectionAttempts: Int { lock.withLock { connections } }
    var advertisementWaits: Int { lock.withLock { waits } }
    var isWaitingForAdvertisement: Bool { lock.withLock { waiting } }

    func connect(within timeout: Duration) async throws -> FakeBootloader {
        try Task.checkCancellation()
        return try lock.withLock {
            let index = connections
            connections += 1
            if failingConnections.contains(index) { throw DFUTransportError.disconnected("no route") }
            var faults = script[min(made.count, script.count - 1)]
            faults.interruptedTransfer = faults.interruptedTransfer || holdsTransfer
            let bootloader = FakeBootloader(faults)
            made.append(bootloader)
            return bootloader
        }
    }

    func waitForAdvertisement() async throws {
        lock.withLock { waits += 1 }
        guard !advertisesAgain else { return }
        lock.withLock { waiting = true }
        defer { lock.withLock { waiting = false } }
        try await Mailbox<Void>().receive()
    }

    func disconnect(_ bootloader: FakeBootloader) {
        bootloader.disconnect()
        let holds = bootloader.holdsTransfer
        lock.withLock { holdsTransfer = holds }
    }
}
