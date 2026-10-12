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
        var policy = RetryPolicy()
        policy.retries = options.retries
        policy.readvertiseTimeout = options.discoveryTimeout
        let succeeded = await HalfUpdates(zips: zips, settings: settings, policy: policy, discoveryTimeout: options.discoveryTimeout).run()
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
    let policy: RetryPolicy
    let discoveryTimeout: Duration

    private enum Outcome: Sendable {
        case updated(peripheral: String, duration: Duration)
        case failed(peripheral: String?, message: String)
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
                        outcomes[missing] = .failed(peripheral: nil, message: "no new AdaDFU appeared within \(seconds). Press OTA on that half and run roamy-ota again with \(zips[missing].name).")
                    }
                    break
                }
                board.begin(index, peripheral: advertiser.shortID, status: "connecting (\(advertiser.name), RSSI \(advertiser.rssi))")
                let half = BluetoothHalf(central: central, advertiser: advertiser)
                let update = HalfUpdate(update: LegacyDFUUpdate(zip: zip, settings: settings), policy: policy)
                group.addTask {
                    (index, await Self.update(update, index: index, on: half, board: board))
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
            case .failed(let peripheral, let message):
                succeeded = false
                let who = peripheral.map { "\(dfuZip.name) (AdaDFU \($0))" } ?? dfuZip.name
                board.say("\(who): FAILED: \(message)")
            }
        }
        return succeeded
    }

    private static func update(_ update: HalfUpdate, index: Int, on half: BluetoothHalf, board: ProgressBoard) async -> Outcome {
        let start = ContinuousClock.now
        do {
            try await update.run(on: half) { event in
                switch event {
                case .connecting(let attempt):
                    if attempt > 1 { board.show(index, status: "connecting (attempt \(attempt))") }
                case .dfu(let event):
                    board.handle(index, event)
                case .retrying(let attempt, let limit, let reason):
                    board.say(index, "starting over, attempt \(attempt) of \(limit), because \(reason.summary).")
                case .resetInterruptedTransfer(let attempt):
                    board.say(index, "the bootloader still held an interrupted transfer, so roamy-ota reset it; attempt \(attempt) starts over.")
                case .waitingForAdvertisement:
                    board.show(index, status: "waiting for the half to advertise as AdaDFU again")
                }
            }
            board.show(index, status: "done")
            return .updated(peripheral: half.advertiser.shortID, duration: .now - start)
        } catch {
            if case .dfu(let failure) = error.reason {
                board.show(index, status: "failed while \(failure.phase)")
            } else {
                board.show(index, status: "failed")
            }
            return .failed(peripheral: half.advertiser.shortID, message: "\(error)")
        }
    }
}

/// One AdaDFU peripheral, which `HalfUpdate` connects to again by its CoreBluetooth identifier.
struct BluetoothHalf: DFUPeripheral {
    let central: BluetoothCentral
    let advertiser: Advertiser

    func connect(within timeout: Duration) async throws -> BluetoothLink {
        try await central.connect(advertiser, timeout: timeout)
    }

    func waitForAdvertisement() async throws {
        try await central.waitForAdvertisement(of: advertiser.id)
    }

    func disconnect(_ link: BluetoothLink) {
        central.disconnect(link)
    }
}
