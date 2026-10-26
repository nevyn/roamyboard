import CoreBluetooth
import Foundation
import RoamyLog

@MainActor let logService = CBUUID(string: LogService.service)
@MainActor let recordsCharacteristic = CBUUID(string: LogService.records)
@MainActor let commandCharacteristic = CBUUID(string: LogService.command)
@MainActor let hidService = CBUUID(string: "1812")

/// Prints a line to stderr, for roamy-log's own state; stdout carries only records and events.
func status(_ message: String) {
    FileHandle.standardError.write(Data("roamy-log: \(message)\n".utf8))
}

func fail(_ message: String) -> Never {
    status(message)
    exit(1)
}

/// Writes each line to stdout and, with --save, appends it to a file.
@MainActor final class Printer {
    private let file: FileHandle?

    init(saveFile: String?) {
        guard let saveFile else {
            file = nil
            return
        }
        if !FileManager.default.fileExists(atPath: saveFile) {
            guard FileManager.default.createFile(atPath: saveFile, contents: nil) else {
                fail("cannot create \(saveFile)")
            }
        }
        guard let handle = FileHandle(forWritingAtPath: saveFile) else { fail("cannot open \(saveFile) for writing") }
        handle.seekToEndOfFile()
        file = handle
    }

    func print(_ line: String) {
        Swift.print(line)
        file?.write(Data((line + "\n").utf8))
    }

    /// Flushes stdout and the save file, then ends the process without running exit()'s teardown.
    func quit(_ code: Int32) -> Never {
        fflush(stdout)
        try? file?.synchronize()
        // exit() runs atexit handlers, which can stall on CoreBluetooth's threads while notifications still arrive.
        _exit(code)
    }
}

/// Finds the keyboard, runs a log session on it, and keeps the session going across dropped links.
///
/// CoreBluetooth calls the delegate methods on the main queue, which owns all of this state.
@MainActor final class LogClient: NSObject {
    private let options: Options
    private let printer: Printer
    private var manager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var records: CBCharacteristic?
    private var command: CBCharacteristic?
    private var reassembler = Reassembler()
    private var tracker = SequenceTracker()
    /// Commands written with response, oldest first, to name the one that a write error belongs to.
    private var pendingWrites: [String] = []
    private var findDeadline: DispatchWorkItem?
    private var stopping = false
    private var sessionCount = 0
    /// --list-modules: when the list is complete, quit.
    private var listedQuitTimer: DispatchWorkItem?
    private var listedAny = false

    init(options: Options) {
        self.options = options
        printer = Printer(saveFile: options.saveFile)
        super.init()
        manager = CBCentralManager(delegate: self, queue: .main)
    }

    // MARK: Finding the keyboard

    private func find() {
        if let peripheral = manager.retrieveConnectedPeripherals(withServices: [logService]).first {
            status("found \(peripheral.name ?? "a keyboard") connected to this Mac, with the log service")
            use(peripheral)
            return
        }
        if let peripheral = manager.retrieveConnectedPeripherals(withServices: [hidService])
            .first(where: { $0.name == options.name }) {
            status("found \"\(options.name)\" connected to this Mac as a keyboard; looking for its log service")
            use(peripheral)
            return
        }
        status("no \"\(options.name)\" is connected to this Mac; scanning for \(options.findTimeoutSeconds) s")
        manager.scanForPeripherals(withServices: nil)
        let deadline = DispatchWorkItem { [weak self] in
            guard let self else { return }
            manager.stopScan()
            fail("""
                no keyboard found: none connected to this Mac offers the log service, and no \
                "\(options.name)" advertised within \(options.findTimeoutSeconds) s. Connect the keyboard to \
                this Mac on its active Bluetooth profile, or name it with --name.
                """)
        }
        findDeadline = deadline
        DispatchQueue.main.asyncAfter(deadline: .now() + .seconds(options.findTimeoutSeconds), execute: deadline)
    }

    private func use(_ found: CBPeripheral) {
        findDeadline?.cancel()
        manager.stopScan()
        peripheral = found
        found.delegate = self
        manager.connect(found)
    }

    // MARK: The session

    private func beginSession() {
        guard let peripheral, let command else { return }
        let start = options.listModules
            ? "live"
            : Commands.start(backlog: options.backlog, resume: tracker.resumePoints)
        var commands = [start] + options.levels.map(\.command)
        if options.listModules { commands.append(Commands.list) }
        for text in commands {
            pendingWrites.append(text)
            peripheral.writeValue(Data(text.utf8), for: command, type: .withResponse)
        }
        sessionCount += 1
        status(sessionCount == 1 ? "reading logs; Ctrl-C stops" : "reading logs again")
        if options.listModules {
            DispatchQueue.main.asyncAfter(deadline: .now() + 10) { [weak self] in
                guard let self, !listedAny else { return }
                fail("no source list arrived within 10 s; the firmware may be older than the list command")
            }
        }
    }

    private func handle(_ record: LogRecord) {
        if options.listModules {
            guard let list = SourceList(record) else { return }
            listedAny = true
            for entry in list.entries { printer.print("\(record.half.letter) \(entry.name)=\(entry.level)") }
            // Each half sends its list in parts; quit once no part has come for a while.
            listedQuitTimer?.cancel()
            let timer = DispatchWorkItem { [weak self] in self?.stop() }
            listedQuitTimer = timer
            DispatchQueue.main.asyncAfter(deadline: .now() + 2, execute: timer)
            return
        }
        for event in tracker.observe(record) where options.half == nil || options.half == half(of: event) {
            printer.print(Output.line(event))
        }
        if options.half == nil || options.half == record.half {
            printer.print(Output.line(record))
        }
    }

    private func half(of event: SequenceTracker.Event) -> Half {
        switch event {
        case let .dropped(half, _), let .restarted(half), let .backlogEnded(half): half
        }
    }

    /// Ends the session, so that the firmware sets its levels back to info, and quits once the keyboard
    /// has acknowledged both the stop command and the unsubscription, or after 1 s.
    func stop() {
        if stopping { printer.quit(0) }
        stopping = true
        // A second Ctrl-C kills the process even if the main thread never gets back to its queue.
        signal(SIGINT, SIG_DFL)
        signal(SIGTERM, SIG_DFL)
        guard let peripheral, peripheral.state == .connected, let command, let records else { printer.quit(0) }
        pendingWrites.append(Commands.stop)
        peripheral.writeValue(Data(Commands.stop.utf8), for: command, type: .withResponse)
        peripheral.setNotifyValue(false, for: records)
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) { [printer] in printer.quit(0) }
    }

    private var stopWritten = false
    private var unsubscribed = false

    private func quitIfStopAcknowledged() {
        if stopWritten && unsubscribed { printer.quit(0) }
    }
}

extension LogClient: @MainActor CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            if let peripheral {
                status("Bluetooth is back; waiting for the keyboard")
                manager.connect(peripheral)
            } else {
                find()
            }
        case .unauthorized:
            fail("Bluetooth access denied; allow the terminal app in System Settings > Privacy & Security > Bluetooth.")
        case .poweredOff:
            status("Bluetooth is off; waiting for it")
        case .unsupported:
            fail("this Mac has no Bluetooth LE")
        default:
            break
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi: NSNumber) {
        let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name
        guard self.peripheral == nil, name == options.name else { return }
        status("found \"\(options.name)\" advertising; connecting")
        use(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([logService])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: (any Error)?) {
        status("could not connect to the keyboard: \(error?.localizedDescription ?? "no reason given"); trying again")
        manager.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral,
                        error: (any Error)?) {
        if stopping { printer.quit(0) }
        records = nil
        command = nil
        pendingWrites.removeAll()
        reassembler.reset()
        status("lost the keyboard (\(error?.localizedDescription ?? "disconnected")); waiting for it to come back")
        manager.connect(peripheral)
    }
}

extension LogClient: @MainActor CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: (any Error)?) {
        if let error { fail("service discovery failed: \(error.localizedDescription)") }
        guard let service = peripheral.services?.first(where: { $0.uuid == logService }) else {
            fail("""
                \(peripheral.name ?? "the keyboard") has no roamyboard log service. Either its firmware is \
                older than the log service, or macOS still has its GATT database from before the update: \
                remove the keyboard in System Settings > Bluetooth and pair it again.
                """)
        }
        peripheral.discoverCharacteristics([recordsCharacteristic, commandCharacteristic], for: service)
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: (any Error)?) {
        if let error { fail("characteristic discovery failed: \(error.localizedDescription)") }
        records = service.characteristics?.first { $0.uuid == recordsCharacteristic }
        command = service.characteristics?.first { $0.uuid == commandCharacteristic }
        guard let records, command != nil else {
            fail("the log service lacks its \(records == nil ? "records" : "command") characteristic; is the firmware from another branch?")
        }
        peripheral.setNotifyValue(true, for: records)
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: (any Error)?) {
        guard characteristic.uuid == recordsCharacteristic else { return }
        if stopping {
            unsubscribed = unsubscribed || !characteristic.isNotifying || error != nil
            quitIfStopAcknowledged()
            return
        }
        if let error {
            fail(describe(error, doing: "subscribing to the records"))
        }
        if characteristic.isNotifying { beginSession() }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        let text = pendingWrites.isEmpty ? "?" : pendingWrites.removeFirst()
        if stopping {
            if text == Commands.stop { stopWritten = true }
            quitIfStopAcknowledged()
            return
        }
        guard let error else { return }
        if let attError = error as? CBATTError, attError.code.rawValue == LogService.badCommandATTError {
            fail("the firmware does not understand the command \"\(text)\"; is it older than this roamy-log?")
        }
        fail(describe(error, doing: "writing the command \"\(text)\""))
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        guard characteristic.uuid == recordsCharacteristic, let value = characteristic.value else { return }
        let incomplete = reassembler.incompleteRecords
        do {
            if let record = try reassembler.feed([UInt8](value)) { handle(record) }
        } catch .unsupportedVersion(let version) {
            fail("the firmware sends log frames of version \(version); this roamy-log reads version \(Fragment.version). Rebuild roamy-log from the firmware's commit.")
        } catch {
            status("dropped a notification: \(error)")
        }
        if reassembler.incompleteRecords != incomplete { status("dropped a record that arrived incomplete") }
    }

    private func describe(_ error: any Error, doing what: String) -> String {
        if let attError = error as? CBATTError {
            switch attError.code {
            case .insufficientEncryption, .insufficientAuthentication, .insufficientEncryptionKeySize:
                return """
                    \(what) failed: the link is not encrypted (\(attError.code)). The log service needs a \
                    keyboard that is paired with this Mac; pair it in System Settings > Bluetooth.
                    """
            default:
                break
            }
        }
        return "\(what) failed: \(error.localizedDescription)"
    }
}
