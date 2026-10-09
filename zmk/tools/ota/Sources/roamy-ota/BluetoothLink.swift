import CoreBluetooth
import RoamyOTA

/// A connection to one AdaDFU peripheral's legacy DFU service.
///
/// CoreBluetooth calls the delegate methods on `queue`, and every other access to the
/// peripheral and the characteristics goes through `queue` too; the mailboxes carry results
/// from there to the async callers.
final class BluetoothLink: NSObject, CBPeripheralDelegate, DFUTransport, @unchecked Sendable {
    let peripheral: CBPeripheral
    private let queue: DispatchQueue

    private let setupSteps = Mailbox<SetupStep>()
    private let writeOutcomes = Mailbox<String?>()
    private let readOutcomes = Mailbox<ReadOutcome>()
    private let notifications = Mailbox<ReadOutcome>()
    private let readiness = Mailbox<Void>()

    // Confined to `queue`.
    private var controlPoint: CBCharacteristic?
    private var packet: CBCharacteristic?
    private var revision: CBCharacteristic?
    private var packetsWritten = 0
    private var closed: DFUTransportError?

    private enum SetupStep: Sendable {
        case connected
        case servicesDiscovered(error: String?)
        case characteristicsDiscovered(error: String?)
        case notificationsEnabled(error: String?)
    }

    private enum ReadOutcome: Sendable {
        case value([UInt8])
        case failed(String)
    }

    init(peripheral: CBPeripheral, queue: DispatchQueue) {
        self.peripheral = peripheral
        self.queue = queue
        super.init()
    }

    // MARK: Called by BluetoothCentral on `queue`

    func didConnect() {
        setupSteps.post(.connected)
    }

    /// Fails every pending and later wait with `.disconnected(reason)`.
    func close(reason: String?) {
        let error = DFUTransportError.disconnected(reason)
        if closed == nil { closed = error }
        setupSteps.close(throwing: error)
        writeOutcomes.close(throwing: error)
        readOutcomes.close(throwing: error)
        notifications.close(throwing: error)
        readiness.close(throwing: error)
    }

    // MARK: Setup

    /// Waits for the connection, finds the DFU service and its characteristics, and subscribes
    /// to the control point.
    /// - Throws: `BluetoothError` or `DFUTransportError.disconnected`.
    func setUp() async throws {
        guard case .connected = try await setupSteps.receive() else { throw BluetoothError.setup("connection event out of order") }

        queue.async { [self] in peripheral.discoverServices([CBUUID(string: LegacyDFUService.service)]) }
        guard case .servicesDiscovered(let error) = try await setupSteps.receive() else { throw BluetoothError.setup("service discovery out of order") }
        if let error { throw BluetoothError.setup("service discovery failed: \(error)") }
        try queue.sync {
            guard let service = peripheral.services?.first(where: { $0.uuid == CBUUID(string: LegacyDFUService.service) }) else {
                throw BluetoothError.setup("the peripheral has no DFU service")
            }
            peripheral.discoverCharacteristics(
                [LegacyDFUService.controlPoint, LegacyDFUService.packet, LegacyDFUService.revision].map { CBUUID(string: $0) },
                for: service)
        }

        guard case .characteristicsDiscovered(let error) = try await setupSteps.receive() else { throw BluetoothError.setup("characteristic discovery out of order") }
        if let error { throw BluetoothError.setup("characteristic discovery failed: \(error)") }
        try queue.sync {
            guard let controlPoint, controlPoint.properties.isSuperset(of: [.write, .notify]) else {
                throw BluetoothError.setup("the DFU control point is missing or cannot be written and notified")
            }
            guard let packet, packet.properties.contains(.writeWithoutResponse) else {
                throw BluetoothError.setup("the DFU packet characteristic is missing or takes no writes without response")
            }
            peripheral.setNotifyValue(true, for: controlPoint)
        }

        guard case .notificationsEnabled(let error) = try await setupSteps.receive() else { throw BluetoothError.setup("notification setup out of order") }
        if let error { throw BluetoothError.setup("subscribing to the control point failed: \(error)") }
    }

    // MARK: DFUTransport

    func readRevision() async throws -> DFURevision? {
        let present: Bool = queue.sync {
            readOutcomes.drain()
            guard let revision else { return false }
            peripheral.readValue(for: revision)
            return true
        }
        guard present else { return nil }
        switch try await readOutcomes.receive() {
        case .value(let bytes):
            guard let revision = DFURevision(bytes: bytes) else {
                throw DFUTransportError.failed("the DFU revision has \(bytes.count) bytes instead of 2")
            }
            return revision
        case .failed(let reason):
            throw DFUTransportError.failed("reading the DFU revision failed: \(reason)")
        }
    }

    func writeControlPoint(_ value: [UInt8]) async throws {
        try queue.sync {
            if let closed { throw closed }
            writeOutcomes.drain()
            peripheral.writeValue(Data(value), for: controlPoint!, type: .withResponse)
        }
        let outcome: String?
        do {
            outcome = try await writeOutcomes.receive()
        } catch DFUTransportError.disconnected(let reason) {
            throw DFUTransportError.droppedBeforeAcknowledgement(reason)
        }
        if let reason = outcome { throw DFUTransportError.writeFailed(reason) }
    }

    func waitUntilReadyToSendPacket() async throws {
        while true {
            let ready: Bool = try queue.sync {
                if let closed { throw closed }
                readiness.drain()
                // canSendWriteWithoutResponse can read false before the first write (Nordic's DFUPacket.swift).
                return peripheral.canSendWriteWithoutResponse || packetsWritten == 0
            }
            if ready { return }
            try await readiness.receive()
        }
    }

    func writePacket(_ value: [UInt8]) async throws {
        try queue.sync {
            if let closed { throw closed }
            peripheral.writeValue(Data(value), for: packet!, type: .withoutResponse)
            packetsWritten += 1
        }
    }

    func nextNotification() async throws -> [UInt8] {
        switch try await notifications.receive() {
        case .value(let bytes): bytes
        case .failed(let reason): throw DFUTransportError.failed("a control point notification failed: \(reason)")
        }
    }

    func maximumPacketLength() async -> Int {
        queue.sync { peripheral.maximumWriteValueLength(for: .withoutResponse) }
    }

    // MARK: CBPeripheralDelegate

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: (any Error)?) {
        setupSteps.post(.servicesDiscovered(error: error?.localizedDescription))
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: (any Error)?) {
        for characteristic in service.characteristics ?? [] {
            switch characteristic.uuid {
            case CBUUID(string: LegacyDFUService.controlPoint): controlPoint = characteristic
            case CBUUID(string: LegacyDFUService.packet): packet = characteristic
            case CBUUID(string: LegacyDFUService.revision): revision = characteristic
            default: break
            }
        }
        setupSteps.post(.characteristicsDiscovered(error: error?.localizedDescription))
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic, error: (any Error)?) {
        guard characteristic == controlPoint else { return }
        setupSteps.post(.notificationsEnabled(error: error?.localizedDescription))
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        guard characteristic == controlPoint else { return }
        writeOutcomes.post(error?.localizedDescription)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        let outcome: ReadOutcome = if let error {
            .failed(error.localizedDescription)
        } else {
            .value([UInt8](characteristic.value ?? Data()))
        }
        if characteristic == controlPoint {
            notifications.post(outcome)
        } else if characteristic == revision {
            readOutcomes.post(outcome)
        }
    }

    func peripheralIsReady(toSendWriteWithoutResponse peripheral: CBPeripheral) {
        readiness.post(())
    }
}

enum BluetoothError: Error, CustomStringConvertible {
    case unauthorized
    case poweredOff
    case unsupported
    case notReady
    case setup(String)

    var description: String {
        switch self {
        case .unauthorized: "macOS denies Bluetooth to this terminal app. Allow it in System Settings > Privacy & Security > Bluetooth, then run roamy-ota again."
        case .poweredOff: "Bluetooth is off. Turn it on and run roamy-ota again."
        case .unsupported: "this Mac has no Bluetooth LE."
        case .notReady: "Bluetooth did not become ready within 60 seconds. If macOS asked whether the terminal app may use Bluetooth, allow it and run roamy-ota again."
        case .setup(let reason): reason
        }
    }
}
