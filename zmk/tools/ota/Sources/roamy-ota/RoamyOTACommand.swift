import Foundation
import RoamyOTA

@main
enum RoamyOTACommand {
    static func main() async {
        let options: Options
        do {
            options = try Options.parse(Array(CommandLine.arguments.dropFirst()))
        } catch {
            fail("\(error)\n\n\(Options.usage)", status: 2)
        }
        if options.help {
            print(Options.usage)
            exit(0)
        }

        var given: [DFUZip] = []
        for input in options.inputs {
            do { given.append(try DFUZip(contentsOf: URL(fileURLWithPath: input))) } catch { fail("\(error)", status: 1) }
        }
        let (zips, reordered) = updateOrder(given)
        if reordered {
            print("Updating \(zips.map(\.name).joined(separator: ", ")) in that order: the left half relays the right half's OTA key, so it goes last.")
        }
        var settings = DFUSettings()
        settings.packetReceiptInterval = options.packetReceiptInterval
        settings.packetSize = options.packetSize

        if options.dryRun {
            print(zips.map { $0.summary(settings: settings) }.joined(separator: "\n\n"))
            exit(0)
        }
        let succeeded = await HalfUpdates(zips: zips, settings: settings, discoveryTimeout: options.discoveryTimeout).run()
        exit(succeeded ? 0 : 1)
    }

    static func fail(_ message: String, status: Int32) -> Never {
        FileHandle.standardError.write("roamy-ota: \(message)\n")
        exit(status)
    }
}

/// Assigns DFU zips to AdaDFU peripherals in discovery order and updates them concurrently.
struct HalfUpdates {
    let zips: [DFUZip]
    let settings: DFUSettings
    let discoveryTimeout: Duration

    private enum Outcome: Sendable {
        case updated(peripheral: String, duration: Duration)
        case failed(peripheral: String?, message: String, firmwareErased: Bool)
    }

    /// - Returns: Whether every half validated and activated its image.
    func run() async -> Bool {
        let board = ProgressBoard(names: zips.map(\.name))
        let central = BluetoothCentral()
        do {
            try await central.waitUntilPoweredOn()
        } catch {
            board.say("roamy-ota: \(error)")
            return false
        }
        central.startScanning()

        var outcomes = [Outcome?](repeating: nil, count: zips.count)
        await withTaskGroup(of: (Int, Outcome).self) { group in
            for (index, zip) in zips.enumerated() {
                board.say("Press OTA on the half for \(zip.name).")
                let advertiser: Advertiser
                do {
                    advertiser = try await withTimeout(discoveryTimeout) { try await central.discoveries.receive() }
                } catch {
                    let seconds = discoveryTimeout.formatted(.units(allowed: [.seconds]))
                    for missing in index..<zips.count {
                        outcomes[missing] = .failed(peripheral: nil, message: "no new AdaDFU appeared within \(seconds). Press OTA on that half and run roamy-ota again with \(zips[missing].name).", firmwareErased: false)
                    }
                    break
                }
                board.begin(index, peripheral: advertiser.shortID, status: "connecting (\(advertiser.name), RSSI \(advertiser.rssi))")
                let settings = settings
                group.addTask {
                    (index, await update(zip, index: index, on: advertiser, central: central, board: board, settings: settings))
                }
            }
            central.stopScanning()
            for await (index, outcome) in group { outcomes[index] = outcome }
        }

        board.say("")
        var succeeded = true
        for (dfuZip, outcome) in zip(zips, outcomes) {
            switch outcome! {
            case .updated(let peripheral, let duration):
                let time = duration.formatted(.time(pattern: .minuteSecond))
                board.say("\(dfuZip.name) (AdaDFU \(peripheral)): updated in \(time); the half validated and activated the new firmware and restarts with it.")
            case .failed(let peripheral, let message, let erased):
                succeeded = false
                let who = peripheral.map { "\(dfuZip.name) (AdaDFU \($0))" } ?? dfuZip.name
                let recovery = erased
                    ? " The half has no firmware now and stays in OTA mode: run roamy-ota again with \(dfuZip.name), or double-tap reset and copy the UF2 (docs/firmware.md, Flashing)."
                    : ""
                board.say("\(who): FAILED: \(message)\(recovery)")
            }
        }
        return succeeded
    }

    private func update(_ zip: DFUZip, index: Int, on advertiser: Advertiser, central: BluetoothCentral, board: ProgressBoard, settings: DFUSettings) async -> Outcome {
        let link: BluetoothLink
        do {
            link = try await central.connect(advertiser, timeout: .seconds(20))
        } catch {
            let reason = error is TimeoutError ? "no connection within 20 seconds" : "\(error)"
            board.finish(index, status: "failed to connect")
            return .failed(peripheral: advertiser.shortID, message: "connecting failed: \(reason). The half stays in OTA mode; run roamy-ota again with \(zip.name).", firmwareErased: false)
        }
        defer { central.disconnect(link) }

        let start = ContinuousClock.now
        do {
            try await LegacyDFUUpdate(zip: zip, settings: settings).run(over: link) { board.handle(index, $0) }
            board.finish(index, status: "done")
            return .updated(peripheral: advertiser.shortID, duration: .now - start)
        } catch {
            board.finish(index, status: "failed while \(error.phase)")
            return .failed(peripheral: advertiser.shortID, message: "\(error)", firmwareErased: error.firmwareErased)
        }
    }
}
