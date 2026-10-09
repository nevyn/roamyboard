import Foundation
import RoamyOTA

/// One status line per half, redrawn in place on a terminal; on a pipe, a line per phase and
/// per 10 % of the upload instead. Messages print above the lines.
final class ProgressBoard: @unchecked Sendable {
    private struct Half {
        let name: String
        var peripheral: String
        var status = "waiting"
        var sent = 0
        var total = 0
        var uploadStart: ContinuousClock.Instant?
        var printedTenths = -1
    }

    private let lock = NSLock()
    private var halves: [Half]
    private var drawnLines = 0
    private var lastDraw = ContinuousClock.now
    private let isTerminal = isatty(STDOUT_FILENO) != 0

    init(names: [String]) {
        halves = names.map { Half(name: $0, peripheral: "") }
    }

    /// Prints `message` above the status lines.
    func say(_ message: String) {
        lock.withLock {
            if isTerminal { clear() }
            print(message)
            if isTerminal { draw() }
        }
    }

    /// Shows that half `index` is AdaDFU `peripheral`, with `status`.
    func begin(_ index: Int, peripheral: String, status: String) {
        lock.withLock {
            halves[index].peripheral = peripheral
            set(index, status: status)
        }
    }

    func handle(_ index: Int, _ event: DFUEvent) {
        lock.withLock {
            switch event {
            case .phase(let phase):
                if phase == .upload { halves[index].uploadStart = .now }
                set(index, status: phase.description)
            case .uploaded(let sent, let total):
                halves[index].sent = sent
                halves[index].total = total
                let line = uploadLine(halves[index])
                halves[index].status = line
                if isTerminal {
                    if ContinuousClock.now - lastDraw > .milliseconds(250) || sent == total { redraw() }
                } else {
                    let tenths = sent * 10 / total
                    if tenths > halves[index].printedTenths {
                        halves[index].printedTenths = tenths
                        print("\(label(halves[index])): \(line)")
                    }
                }
            }
        }
    }

    func finish(_ index: Int, status: String) {
        lock.withLock { set(index, status: status) }
    }

    // MARK: Drawing, with `lock` held

    private func set(_ index: Int, status: String) {
        halves[index].status = status
        if isTerminal { redraw() } else { print("\(label(halves[index])): \(status)") }
    }

    private func label(_ half: Half) -> String {
        half.peripheral.isEmpty ? half.name : "\(half.name) (AdaDFU \(half.peripheral))"
    }

    private func uploadLine(_ half: Half) -> String {
        let percent = half.sent * 100 / max(half.total, 1)
        var line = "uploading \(half.sent.formatted()) / \(half.total.formatted()) B  \(percent) %"
        if let start = half.uploadStart {
            let elapsed = (ContinuousClock.now - start) / .seconds(1)
            if elapsed > 1, half.sent > 0 {
                let rate = Double(half.sent) / elapsed
                let remaining = Double(half.total - half.sent) / rate
                line += String(format: "  %.1f kB/s  ETA %d:%02d", rate / 1000, Int(remaining) / 60, Int(remaining) % 60)
            }
        }
        return line
    }

    private func clear() {
        if drawnLines > 0 { FileHandle.standardOutput.write("\u{1B}[\(drawnLines)A\u{1B}[J") }
        drawnLines = 0
    }

    private func draw() {
        for half in halves where !half.peripheral.isEmpty || half.status != "waiting" {
            print("\u{1B}[2K\(label(half)): \(half.status)")
            drawnLines += 1
        }
        lastDraw = .now
    }

    private func redraw() {
        clear()
        draw()
    }
}

extension FileHandle {
    func write(_ string: String) { write(Data(string.utf8)) }
}
