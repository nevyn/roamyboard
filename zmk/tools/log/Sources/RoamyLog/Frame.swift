/// The roamyboard log service's GATT identifiers and its frame format, as zmk/src/log/frame.h
/// writes it; docs/logging.md states the contract.
public enum LogService {
    public static let service = "7BEBA2B0-68D2-4D86-A34C-647827C1B55B"
    /// Notify: one fragment of a record per notification.
    public static let records = "7BEBA2B1-68D2-4D86-A34C-647827C1B55B"
    /// Write: one command per write.
    public static let command = "7BEBA2B2-68D2-4D86-A34C-647827C1B55B"
    /// The ATT error that a command the firmware does not understand gets.
    public static let badCommandATTError = 0x80
}

/// The half that recorded a record.
public enum Half: UInt8, Sendable, CaseIterable, Comparable {
    case unibody = 0
    case left = 1
    case right = 2

    /// "U", "L" or "R", as in the output and in start commands.
    public var letter: String {
        switch self {
        case .unibody: "U"
        case .left: "L"
        case .right: "R"
        }
    }

    public init?(letter: String) {
        guard let half = Half.allCases.first(where: { $0.letter == letter.uppercased() }) else { return nil }
        self = half
    }

    public static func < (a: Half, b: Half) -> Bool { a.rawValue < b.rawValue }
}

/// One log record, as a half recorded it.
public struct LogRecord: Equatable, Sendable {
    public var half: Half
    /// Counts this half's records from 0 at its start.
    public var seq: UInt32
    /// The half recorded it before the session began.
    public var backlog: Bool
    /// Milliseconds since the half started.
    public var uptimeMs: UInt32
    /// Zephyr's level: 0 printk, 1 error, 2 warning, 3 info, 4 debug.
    public var level: UInt8
    /// The firmware cut the text to 192 bytes.
    public var truncated: Bool
    /// The log source (module), such as "zmk"; empty for printk.
    public var source: String
    public var text: String

    public init(half: Half, seq: UInt32, backlog: Bool, uptimeMs: UInt32, level: UInt8,
                truncated: Bool = false, source: String, text: String) {
        self.half = half
        self.seq = seq
        self.backlog = backlog
        self.uptimeMs = uptimeMs
        self.level = level
        self.truncated = truncated
        self.source = source
        self.text = text
    }
}

/// Why a notification did not become part of a record.
public enum FrameError: Error, Equatable, Sendable {
    /// Shorter than the 6-byte header.
    case tooShort(length: Int)
    /// A frame format version that this roamy-log does not know.
    case unsupportedVersion(Int)
    /// A continuation that does not follow the record in progress.
    case outOfOrder(half: Half, seq: UInt32, index: Int)
    /// A record longer than any that the firmware writes.
    case tooLong(half: Half, seq: UInt32)
    /// A complete record whose body does not parse.
    case malformedBody(half: Half, seq: UInt32)
}

/// One notification: a header, then a piece of a record's body.
public struct Fragment: Equatable, Sendable {
    public static let headerLength = 6
    public static let version = 1

    public var half: Half
    public var last: Bool
    public var backlog: Bool
    public var index: Int
    public var seq: UInt32
    public var payload: [UInt8]

    /// Parses a notification's bytes.
    /// - Throws: `FrameError.tooShort` or `.unsupportedVersion`.
    public init(_ bytes: [UInt8]) throws(FrameError) {
        guard bytes.count >= Fragment.headerLength else { throw .tooShort(length: bytes.count) }
        let flags = bytes[0]
        guard Int(flags >> 4) == Fragment.version else { throw .unsupportedVersion(Int(flags >> 4)) }
        // Half 3 is unused; it reads as the unibody rather than failing the record.
        half = Half(rawValue: flags & 0x03) ?? .unibody
        last = flags & 0x04 != 0
        backlog = flags & 0x08 != 0
        index = Int(bytes[1])
        seq = UInt32(littleEndian: bytes[2...5])
        payload = Array(bytes[6...])
    }
}

extension UInt32 {
    init(littleEndian bytes: ArraySlice<UInt8>) {
        self = bytes.reversed().reduce(0) { $0 << 8 | UInt32($1) }
    }
}

/// Puts fragments back together into records. The firmware sends a record's fragments one after
/// another, so one record is in progress at a time.
public struct Reassembler: Sendable {
    /// The longest body that the firmware writes: 6 fixed bytes, a 32-byte source, 192 bytes of text.
    public static let bodyMax = 230

    private var current: (first: Fragment, body: [UInt8], nextIndex: Int)?
    /// Records whose first fragments arrived but which another record's first fragment cut short.
    public private(set) var incompleteRecords = 0

    public init() {}

    /// Feeds one notification.
    /// - Returns: The record that it completes, or nil while the record is still partial.
    /// - Throws: `FrameError`; the record in progress is dropped.
    public mutating func feed(_ bytes: [UInt8]) throws(FrameError) -> LogRecord? {
        let fragment: Fragment
        do {
            fragment = try Fragment(bytes)
        } catch {
            current = nil
            throw error
        }
        if fragment.index == 0 {
            if current != nil { incompleteRecords += 1 }
            current = (fragment, [], 0)
        }
        guard var record = current, record.nextIndex == fragment.index, record.first.seq == fragment.seq,
              record.first.half == fragment.half else {
            current = nil
            throw .outOfOrder(half: fragment.half, seq: fragment.seq, index: fragment.index)
        }
        record.body += fragment.payload
        record.nextIndex += 1
        guard record.body.count <= Reassembler.bodyMax else {
            current = nil
            throw .tooLong(half: fragment.half, seq: fragment.seq)
        }
        guard fragment.last else {
            current = record
            return nil
        }
        current = nil
        guard let parsed = Reassembler.record(from: record.first, body: record.body) else {
            throw .malformedBody(half: fragment.half, seq: fragment.seq)
        }
        return parsed
    }

    /// Forgets the record in progress, as when the connection drops.
    public mutating func reset() {
        if current != nil { incompleteRecords += 1 }
        current = nil
    }

    static func record(from first: Fragment, body: [UInt8]) -> LogRecord? {
        guard body.count >= 6 else { return nil }
        let sourceLength = Int(body[5])
        guard body.count >= 6 + sourceLength else { return nil }
        return LogRecord(
            half: first.half,
            seq: first.seq,
            backlog: first.backlog,
            uptimeMs: UInt32(littleEndian: body[0...3]),
            level: body[4] & 0x07,
            truncated: body[4] & 0x80 != 0,
            source: String(decoding: body[6..<(6 + sourceLength)], as: UTF8.self),
            text: String(decoding: body[(6 + sourceLength)...], as: UTF8.self)
        )
    }
}
