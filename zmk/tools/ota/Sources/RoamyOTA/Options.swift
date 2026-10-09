/// The command line of `roamy-ota`.
public struct Options: Equatable, Sendable {
    /// DFU zips, or directories that hold one each, in the order that halves get them.
    public var inputs: [String] = []
    public var packetReceiptInterval: PacketReceiptInterval = .default
    /// Bytes per image packet; see `DFUSettings.packetSize`.
    public var packetSize = 20
    /// How long to wait for each half to appear as AdaDFU.
    public var discoveryTimeout: Duration = .seconds(120)
    /// Parse and describe the zips without using Bluetooth.
    public var dryRun = false
    public var help = false

    public init() {}

    public static let usage = """
        usage: roamy-ota [--prn N] [--timeout SECONDS] [--packet-size BYTES] [--dry-run] ZIP [ZIP ...]

        Updates roamyboard halves over Bluetooth. Run it, then press OTA on each half
        when it asks: the first half that appears as AdaDFU gets the first ZIP, the next
        one the second. A ZIP whose name contains "left" goes last, since the left half
        relays the right half's OTA key. Halves update at the same time.

        ZIP is a DFU zip from a CI build, or a directory that holds one, such as an
        artifact fetched with `gh run download RUN -n roamyboard_left`.

          --prn N              packets per receipt, 1 to 8 (default 8)
          --timeout SECONDS    how long to wait for each half (default 120)
          --dry-run            describe what would be sent; no Bluetooth
          --packet-size BYTES  bytes per packet, a multiple of 4 (default 20;
                               larger is untested on the roamyboard)
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
        func integer(for option: String) throws(OptionError) -> Int {
            let text = try value(for: option)
            guard let number = Int(text) else { throw .invalidValue(option: option, value: text, reason: "it is not a whole number") }
            return number
        }
        while let argument = remaining.popFirst() {
            switch argument {
            case "--prn":
                options.packetReceiptInterval = try PacketReceiptInterval(integer(for: argument))
            case "--packet-size":
                let size = try integer(for: argument)
                guard size >= 20, size % 4 == 0 else {
                    throw .invalidValue(option: argument, value: String(size), reason: "the bootloader takes whole 4-byte words, and every link fits 20; use 20, 24, 28 and so on")
                }
                options.packetSize = size
            case "--timeout":
                let seconds = try integer(for: argument)
                guard seconds > 0 else { throw .invalidValue(option: argument, value: String(seconds), reason: "it must be at least 1 second") }
                options.discoveryTimeout = .seconds(seconds)
            case "--dry-run":
                options.dryRun = true
            case "-h", "--help":
                options.help = true
            case let flag where flag.hasPrefix("-"):
                throw .unknownOption(flag)
            default:
                options.inputs.append(argument)
            }
        }
        if options.inputs.isEmpty, !options.help { throw .noInputs }
        return options
    }
}

public enum OptionError: Error, Equatable, CustomStringConvertible {
    case unknownOption(String)
    case missingValue(option: String)
    case invalidValue(option: String, value: String, reason: String)
    case noInputs

    public var description: String {
        switch self {
        case .unknownOption(let option): "unknown option \(option)"
        case .missingValue(let option): "\(option) needs a value"
        case .invalidValue(let option, let value, let reason): "\(option) \(value) is not allowed: \(reason)"
        case .noInputs: "name at least one DFU zip"
        }
    }
}
