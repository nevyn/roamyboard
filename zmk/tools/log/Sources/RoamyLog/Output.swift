import Foundation

/// How roamy-log writes records and events, one line each.
public enum Output {
    /// "inf" for 3; "raw" for printk and unknown levels.
    public static func levelName(_ level: UInt8) -> String {
        switch level {
        case 1: "err"
        case 2: "wrn"
        case 3: "inf"
        case 4: "dbg"
        default: "raw"
        }
    }

    /// The half's uptime as hours:minutes:seconds.milliseconds, such as "00:12:34.567".
    public static func uptime(_ ms: UInt32) -> String {
        let hours = ms / 3_600_000
        let minutes = ms / 60_000 % 60
        let seconds = ms / 1000 % 60
        func pad(_ value: UInt32, _ width: Int) -> String {
            let digits = String(value)
            return String(repeating: "0", count: max(0, width - digits.count)) + digits
        }
        return "\(pad(hours, 2)):\(pad(minutes, 2)):\(pad(seconds, 2)).\(pad(ms % 1000, 3))"
    }

    /// "L 00:12:34.567 inf zmk: Welcome to ZMK!". A text with line breaks continues on indented
    /// lines; a cut text ends in " [cut]".
    public static func line(_ record: LogRecord) -> String {
        var prefix = "\(record.half.letter) \(uptime(record.uptimeMs)) \(levelName(record.level)) "
        if !record.source.isEmpty { prefix += "\(record.source): " }
        let text = record.text.replacingOccurrences(of: "\r\n", with: "\n")
            .replacingOccurrences(of: "\n", with: "\n    ")
        return prefix + text + (record.truncated ? " [cut]" : "")
    }

    /// "L -- 3 records dropped --".
    public static func line(_ event: SequenceTracker.Event) -> String {
        switch event {
        case let .dropped(half, count):
            "\(half.letter) -- \(count) record\(count == 1 ? "" : "s") dropped --"
        case let .restarted(half):
            "\(half.letter) -- restarted --"
        case let .backlogEnded(half):
            "\(half.letter) -- end of backlog; live from here --"
        }
    }
}

/// The firmware's answer to the list command: notes from source "roamy_log" such as
/// "sources 1-12 of 40: zmk=inf bt_hci_core=inf ...".
public struct SourceList: Equatable, Sendable {
    public var first: Int
    public var last: Int
    public var total: Int
    /// Each source and its level, in the firmware's order.
    public var entries: [(name: String, level: String)]

    /// Parses a record of the list; nil for any other record.
    public init?(_ record: LogRecord) {
        guard record.source == "roamy_log", record.text.hasPrefix("sources "),
              let colon = record.text.firstIndex(of: ":") else { return nil }
        let range = record.text[record.text.index(record.text.startIndex, offsetBy: 8)..<colon]
            .split(separator: " ")
        guard range.count == 3, range[1] == "of", let total = Int(range[2]) else { return nil }
        let bounds = range[0].split(separator: "-")
        guard bounds.count == 2, let first = Int(bounds[0]), let last = Int(bounds[1]) else { return nil }
        self.first = first
        self.last = last
        self.total = total
        entries = record.text[record.text.index(after: colon)...].split(separator: " ").compactMap { pair in
            let parts = pair.split(separator: "=", maxSplits: 1)
            return parts.count == 2 ? (String(parts[0]), String(parts[1])) : nil
        }
    }

    public static func == (a: SourceList, b: SourceList) -> Bool {
        a.first == b.first && a.last == b.last && a.total == b.total
            && a.entries.map(\.name) == b.entries.map(\.name) && a.entries.map(\.level) == b.entries.map(\.level)
    }
}
