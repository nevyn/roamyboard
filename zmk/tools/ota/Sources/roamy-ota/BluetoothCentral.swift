import CoreBluetooth
import RoamyOTA

/// A peripheral that advertises the legacy DFU service, seen for the first time.
struct Advertiser: Sendable {
    let id: UUID
    let name: String
    let rssi: Int

    /// The first block of the identifier that macOS gives the peripheral, to tell halves apart.
    var shortID: String { String(id.uuidString.prefix(8)) }
}

/// Finds AdaDFU peripherals and connects to them. Creating one asks macOS for Bluetooth access.
///
/// CoreBluetooth calls the delegate methods on `queue`; the state below is confined to it.
final class BluetoothCentral: NSObject, CBCentralManagerDelegate, @unchecked Sendable {
    private let queue = DispatchQueue(label: "roamy-ota.bluetooth")
    private var manager: CBCentralManager!
    private let states = Mailbox<CBManagerState>()

    /// Each AdaDFU peripheral once, in the order that they are first seen.
    let discoveries = Mailbox<Advertiser>()

    private var seen: [UUID: CBPeripheral] = [:]
    private var links: [UUID: BluetoothLink] = [:]

    override init() {
        super.init()
        manager = CBCentralManager(delegate: self, queue: queue)
    }

    /// Returns once Bluetooth is on and this process may use it.
    /// - Throws: `BluetoothError` if access is denied, Bluetooth is off or missing, or the state
    ///   stays unknown for 60 seconds, which leaves time to answer macOS's permission prompt.
    func waitUntilPoweredOn() async throws {
        let states = states
        do {
            try await withTimeout(.seconds(60)) {
                while true {
                    switch try await states.receive() {
                    case .poweredOn: return
                    case .unauthorized: throw BluetoothError.unauthorized
                    case .poweredOff: throw BluetoothError.poweredOff
                    case .unsupported: throw BluetoothError.unsupported
                    default: continue
                    }
                }
            }
        } catch is TimeoutError {
            throw BluetoothError.notReady
        }
    }

    func startScanning() {
        queue.async { [self] in
            manager.scanForPeripherals(withServices: [CBUUID(string: LegacyDFUService.service)])
        }
    }

    func stopScanning() {
        queue.async { [self] in manager.stopScan() }
    }

    /// Connects to `advertiser` and prepares its DFU service.
    /// - Throws: `BluetoothError`, `DFUTransportError.disconnected`, or `TimeoutError` after
    ///   `timeout`; the connection attempt is cancelled in each case.
    func connect(_ advertiser: Advertiser, timeout: Duration) async throws -> BluetoothLink {
        let link: BluetoothLink = queue.sync {
            let peripheral = seen[advertiser.id]!
            let link = BluetoothLink(peripheral: peripheral, queue: queue)
            peripheral.delegate = link
            links[advertiser.id] = link
            manager.connect(peripheral)
            return link
        }
        do {
            try await withTimeout(timeout) { try await link.setUp() }
            return link
        } catch {
            disconnect(link)
            throw error
        }
    }

    /// Drops the connection; pending waits on `link` fail with `.disconnected`.
    func disconnect(_ link: BluetoothLink) {
        queue.async { [self] in
            manager.cancelPeripheralConnection(link.peripheral)
            link.close(reason: "roamy-ota disconnected")
            links[link.peripheral.identifier] = nil
        }
    }

    // MARK: CBCentralManagerDelegate

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        states.post(central.state)
        if central.state != .poweredOn {
            for link in links.values { link.close(reason: "Bluetooth stopped (state \(central.state.rawValue))") }
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi: NSNumber) {
        guard seen[peripheral.identifier] == nil else { return }
        seen[peripheral.identifier] = peripheral
        let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name ?? "unnamed"
        discoveries.post(Advertiser(id: peripheral.identifier, name: name, rssi: rssi.intValue))
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        links[peripheral.identifier]?.didConnect()
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: (any Error)?) {
        links[peripheral.identifier]?.close(reason: error?.localizedDescription ?? "the connection attempt failed")
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: (any Error)?) {
        links[peripheral.identifier]?.close(reason: error?.localizedDescription)
    }
}
