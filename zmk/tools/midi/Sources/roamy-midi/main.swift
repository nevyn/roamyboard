import CoreBluetooth
import CoreMIDI
import Foundation

let usage = """
    usage: roamy-midi [--listen] [--name NAME]

    Finds the roamyboard among the peripherals that this Mac is connected to, hands its BLE-MIDI
    service to CoreMIDI (MIDIBluetoothDriverActivateAllConnections), then prints every MIDI
    message from every MIDI source until Ctrl-C.

      --listen     Only print MIDI; leave Bluetooth alone. Shows whether macOS brought the
                   keyboard's MIDI online by itself.
      --name NAME  The Bluetooth name to look for, if no peripheral offers BLE-MIDI. Default
                   "roamyboard".
    """

let midiService = CBUUID(string: "03B80E5A-EDE8-4B33-A751-6CE34EC4C700")
let hidService = CBUUID(string: "1812")

var arguments = Array(CommandLine.arguments.dropFirst())
var listenOnly = false
var name = "roamyboard"
while let argument = arguments.first {
    arguments.removeFirst()
    switch argument {
    case "--listen": listenOnly = true
    case "--name" where !arguments.isEmpty: name = arguments.removeFirst()
    case "-h", "--help":
        print(usage)
        exit(0)
    default:
        FileHandle.standardError.write(Data("roamy-midi: unknown argument \(argument)\n\n\(usage)\n".utf8))
        exit(64)
    }
}

func fail(_ message: String) -> Never {
    FileHandle.standardError.write(Data("roamy-midi: \(message)\n".utf8))
    exit(1)
}

// MARK: - Printing MIDI

let noteNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]
func noteName(_ note: UInt32) -> String { "\(noteNames[Int(note % 12)])\(Int(note / 12) - 1)" }

/// One MIDI 1.0 channel voice message in a Universal MIDI Packet word, in words.
func describe(_ word: UInt32) -> String {
    let status = (word >> 16) & 0xFF
    let channel = (status & 0x0F) + 1
    let data1 = (word >> 8) & 0x7F
    let data2 = word & 0x7F
    switch status & 0xF0 {
    case 0x90 where data2 > 0: return "note on   ch \(channel) \(noteName(data1)) (\(data1)) velocity \(data2)"
    case 0x80, 0x90: return "note off  ch \(channel) \(noteName(data1)) (\(data1))"
    case 0xB0: return "control   ch \(channel) CC \(data1) = \(data2)"
    case 0xE0: return "bend      ch \(channel) \(Int(data2 << 7 | data1) - 8192)"
    default: return String(format: "raw       %08x", word)
    }
}

let timebase: mach_timebase_info_data_t = {
    var info = mach_timebase_info_data_t()
    mach_timebase_info(&info)
    return info
}()

func milliseconds(_ hostTime: MIDITimeStamp) -> Double {
    Double(hostTime) * Double(timebase.numer) / Double(timebase.denom) / 1_000_000
}

func displayName(_ object: MIDIObjectRef) -> String {
    var name: Unmanaged<CFString>?
    guard MIDIObjectGetStringProperty(object, kMIDIPropertyDisplayName, &name) == noErr, let name else {
        return "source \(object)"
    }
    return name.takeRetainedValue() as String
}

/// The previous event's time; only CoreMIDI's receive thread touches it.
nonisolated(unsafe) var previous: Double?

/// Prints each event with the source's name and the gap since the previous event, from
/// CoreMIDI's timestamps, which BLE-MIDI derives from the keyboard's own.
func printEvents(_ list: UnsafePointer<MIDIEventList>, source: MIDIEndpointRef) {
    let sourceName = displayName(source)
    for packet in list.unsafeSequence() {
        let time = milliseconds(packet.pointee.timeStamp)
        let gap = previous.map { String(format: "+%6.1f ms", time - $0) } ?? "          "
        previous = time
        let count = Int(packet.pointee.wordCount)
        withUnsafeBytes(of: packet.pointee.words) { raw in
            for word in raw.bindMemory(to: UInt32.self).prefix(count) where word >> 28 == 0x2 {
                print("\(gap)  \(sourceName): \(describe(word))")
            }
        }
    }
}

// MARK: - CoreMIDI

@MainActor final class Listener {
    private var client = MIDIClientRef()
    private var port = MIDIPortRef()
    private var connected: Set<MIDIEndpointRef> = []

    init() {
        var status = MIDIClientCreateWithBlock("roamy-midi" as CFString, &client) { @Sendable notification in
            guard notification.pointee.messageID == .msgSetupChanged else { return }
            Task { @MainActor in listener.connectSources() }
        }
        guard status == noErr else { fail("MIDIClientCreate failed: \(status)") }
        status = MIDIInputPortCreateWithProtocol(client, "in" as CFString, ._1_0, &port) { @Sendable list, refCon in
            printEvents(list, source: MIDIEndpointRef(UInt(bitPattern: refCon)))
        }
        guard status == noErr else { fail("MIDIInputPortCreate failed: \(status)") }
        connectSources()
    }

    /// Listens to every source not yet listened to, and names the new ones.
    func connectSources() {
        for index in 0..<MIDIGetNumberOfSources() {
            let source = MIDIGetSource(index)
            guard source != 0, !connected.contains(source) else { continue }
            let status = MIDIPortConnectSource(port, source, UnsafeMutableRawPointer(bitPattern: UInt(source)))
            if status == noErr {
                connected.insert(source)
                print("Listening to MIDI source \"\(displayName(source))\"")
            } else {
                print("Cannot listen to \"\(displayName(source))\": \(status)")
            }
        }
    }
}

// MARK: - CoreBluetooth

@MainActor final class Activator: NSObject {
    private var manager: CBCentralManager!
    private var pending: Set<UUID> = []

    override init() {
        super.init()
        manager = CBCentralManager(delegate: self, queue: .main)
    }

    private func start() {
        var peripherals = manager.retrieveConnectedPeripherals(withServices: [midiService])
        if peripherals.isEmpty {
            print("No connected peripheral lists BLE-MIDI; looking for \"\(name)\" among HID peripherals.")
            peripherals = manager.retrieveConnectedPeripherals(withServices: [hidService])
                .filter { $0.name == name }
        }
        guard !peripherals.isEmpty else {
            print("""
                No connected "\(name)" found. Check that the keyboard is connected to this Mac on its \
                active profile. Listening anyway.
                """)
            return
        }
        for peripheral in peripherals {
            print("Connecting to \(peripheral.name ?? "unnamed") \(peripheral.identifier)")
            pending.insert(peripheral.identifier)
            manager.connect(peripheral)
        }
    }
}

extension Activator: @MainActor CBCentralManagerDelegate, @MainActor CBPeripheralDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn: start()
        case .unauthorized: fail("Bluetooth access denied; allow the terminal app in System Settings > Privacy & Security > Bluetooth.")
        case .poweredOff: fail("Bluetooth is off.")
        case .unsupported: fail("This Mac has no Bluetooth LE.")
        default: break
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.delegate = self
        peripheral.discoverServices(nil)
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: (any Error)?) {
        print("Could not connect to \(peripheral.identifier): \(error?.localizedDescription ?? "no reason given")")
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: (any Error)?) {
        let services = peripheral.services?.map(\.uuid) ?? []
        print("\(peripheral.name ?? "unnamed") offers \(services.map(\.uuidString).joined(separator: ", "))")
        guard services.contains(midiService) else {
            print("""
                It has no BLE-MIDI service. Either its firmware lacks it, or macOS still has the \
                GATT database from before: remove the keyboard in System Settings > Bluetooth and pair again.
                """)
            manager.cancelPeripheralConnection(peripheral)
            return
        }
        let status = MIDIBluetoothDriverActivateAllConnections()
        print("MIDIBluetoothDriverActivateAllConnections: \(status == noErr ? "ok" : "error \(status)")")
        // CoreMIDI keeps its own connection; this process lets go of its own, as Apple's steps say.
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) { [self] in
            manager.cancelPeripheralConnection(peripheral)
            listener.connectSources()
        }
    }
}

// MARK: - Main

setvbuf(stdout, nil, _IOLBF, 0)
let listener = Listener()
let activator: Activator? = listenOnly ? nil : Activator()
print("Play some notes; Ctrl-C quits.")
// dispatchMain() would drain the main queue on another thread, which @MainActor delegate callbacks reject.
RunLoop.main.run()
