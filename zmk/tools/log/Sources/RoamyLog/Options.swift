/// A level for one log source, or for all: "zmk=dbg", "all=inf".
public struct LevelSetting: Equatable, Sendable {
    public static let levels = ["off", "err", "wrn", "inf", "dbg"]

    /// A log source's name, or "all".
    public var module: String
    /// One of `levels`.
    public var level: String

    /// The command that the firmware takes for it.
    public var command: String { "\(module)=\(level)" }

    /// Parses "MODULE=LEVEL".
    /// - Throws: `OptionError.invalidValue` naming what is wrong.
    public init(parsing text: String) throws(OptionError) {
        let parts = text.split(separator: "=", maxSplits: 1, omittingEmptySubsequences: false)
        guard parts.count == 2 else {
            throw .invalidValue(option: "--level", value: text, reason: "it is not MODULE=LEVEL")
        }
        module = String(parts[0])
        level = String(parts[1])
        let nameCharacters = Set("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")
        guard !module.isEmpty, module.utf8.count <= 32, module.allSatisfy(nameCharacters.contains) else {
            throw .invalidValue(option: "--level", value: text,
                                reason: "MODULE must be 1 to 32 letters, digits, _ . or -, or \"all\"")
        }
        guard LevelSetting.levels.contains(level) else {
            throw .invalidValue(option: "--level", value: text,
                                reason: "LEVEL must be one of \(LevelSetting.levels.joined(separator: ", "))")
        }
    }
}

/// The command line of `roamy-log`.
public struct Options: Equatable, Sendable {
    public var levels: [LevelSetting] = []
    /// Show only this half's records; nil shows every half's.
    public var half: Half?
    /// Append every line to this file as well.
    public var saveFile: String?
    public var backlog = true
    public var listModules = false
    /// The Bluetooth name to look for when no connected peripheral offers the log service.
    public var name = "roamyboard"
    /// How long to look for the keyboard before giving up.
    public var findTimeoutSeconds = 15
    public var help = false

    public init() {}

    public static let usage = """
        usage: roamy-log [--level MODULE=LEVEL ...] [--half L|R|U] [--save FILE] [--no-backlog]
                         [--list-modules] [--name NAME] [--timeout SECONDS]

        Prints a roamyboard's logs, from both halves of a split, like tail -f: first the
        backlog that each half kept in RAM, then every new record, one line each:

            L 00:12:34.567 inf zmk: Welcome to ZMK!

        with the half (L, R, or U for a unibody), its uptime, the level and the source.
        Records that never arrived show as "L -- 3 records dropped --". When the link
        drops, roamy-log waits for the keyboard and goes on where it left off.

        The keyboard must be paired with this Mac: the log service requires an encrypted
        link. Run roamy-log from a terminal app, which macOS asks for Bluetooth access the
        first time; macOS kills a Bluetooth client that it cannot attribute to an app, such
        as one started from an agent's shell.

          --level MODULE=LEVEL  set the level of one log source, or of all with "all",
                                on every half: off, err, wrn, inf or dbg. Repeatable;
                                applied in order. Levels go back to inf when roamy-log
                                stops or the link drops, and roamy-log sets them again
                                when it reconnects.
          --half L|R|U          show only that half's records
          --save FILE           also append every line to FILE
          --no-backlog          skip the backlog; show only new records
          --list-modules        list every half's log sources with their levels, then quit
          --name NAME           the keyboard's Bluetooth name, used when no connected
                                peripheral offers the log service (default "roamyboard")
          --timeout SECONDS     how long to look for the keyboard (default 15)
        """

    /// Parses `arguments`, without the program name.
    /// - Throws: `OptionError` naming the offending argument.
    public static func parse(_ arguments: [String]) throws(OptionError) -> Options {
        var options = Options()
        var remaining = arguments[...]
        func value(for option: String) throws(OptionError) -> String {
            guard let value = remaining.popFirst() else { throw .missingValue(option: option) }
            return value
        }
        while let argument = remaining.popFirst() {
            switch argument {
            case "--level":
                options.levels.append(try LevelSetting(parsing: value(for: argument)))
            case "--half":
                let text = try value(for: argument)
                guard let half = Half(letter: text) else {
                    throw .invalidValue(option: argument, value: text, reason: "it is not L, R or U")
                }
                options.half = half
            case "--save":
                options.saveFile = try value(for: argument)
            case "--no-backlog":
                options.backlog = false
            case "--list-modules":
                options.listModules = true
            case "--name":
                options.name = try value(for: argument)
            case "--timeout":
                let text = try value(for: argument)
                guard let seconds = Int(text), seconds > 0 else {
                    throw .invalidValue(option: argument, value: text, reason: "it is not a positive whole number")
                }
                options.findTimeoutSeconds = seconds
            case "-h", "--help":
                options.help = true
            default:
                throw .unknownArgument(argument)
            }
        }
        return options
    }
}

/// A command line that `Options.parse` rejects.
public enum OptionError: Error, Equatable, CustomStringConvertible {
    case unknownArgument(String)
    case missingValue(option: String)
    case invalidValue(option: String, value: String, reason: String)

    public var description: String {
        switch self {
        case let .unknownArgument(argument): "unknown argument \(argument)"
        case let .missingValue(option): "\(option) needs a value"
        case let .invalidValue(option, value, reason): "\(option) \(value): \(reason)"
        }
    }
}

/// The commands that roamy-log writes to the log service's command characteristic.
public enum Commands {
    /// Begins a session. With resume points, each half that has one sends from that record,
    /// and the others from their oldest; without, the whole backlog, or nothing old if
    /// `backlog` is false.
    public static func start(backlog: Bool, resume: [Half: UInt32]) -> String {
        if resume.isEmpty { return backlog ? "start" : "live" }
        let points = resume.sorted { $0.key < $1.key }.map { "\($0.key.letter):\($0.value)" }
        return (["start"] + points).joined(separator: " ")
    }

    public static let stop = "stop"
    public static let list = "list"
}
