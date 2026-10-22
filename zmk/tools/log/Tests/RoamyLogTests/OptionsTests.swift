import Testing
@testable import RoamyLog

@Suite struct OptionsTests {
    @Test func parsesEveryOption() throws {
        let options = try Options.parse(["--level", "all=dbg", "--level", "bt_hci_core=wrn", "--half", "r",
                                         "--save", "/tmp/x.log", "--no-backlog", "--name", "roamy", "--timeout", "5"])
        #expect(options.levels.map(\.command) == ["all=dbg", "bt_hci_core=wrn"])
        #expect(options.half == .right)
        #expect(options.saveFile == "/tmp/x.log")
        #expect(!options.backlog)
        #expect(options.name == "roamy")
        #expect(options.findTimeoutSeconds == 5)
        #expect(try Options.parse(["--list-modules"]).listModules)
        #expect(try Options.parse([]) == Options())
    }

    @Test func namesWhatIsWrong() {
        #expect(throws: OptionError.missingValue(option: "--level")) { try Options.parse(["--level"]) }
        #expect(throws: OptionError.unknownArgument("--follow")) { try Options.parse(["--follow"]) }
        #expect(throws: OptionError.invalidValue(option: "--half", value: "X", reason: "it is not L, R or U")) {
            try Options.parse(["--half", "X"])
        }
        #expect(throws: OptionError.self) { try Options.parse(["--level", "zmk"]) }
        #expect(throws: OptionError.self) { try Options.parse(["--level", "zmk=debug"]) }
        #expect(throws: OptionError.self) { try Options.parse(["--level", "z mk=dbg"]) }
        #expect(throws: OptionError.self) { try Options.parse(["--level", "=dbg"]) }
        #expect(throws: OptionError.self) { try Options.parse(["--timeout", "0"]) }
    }

    @Test func startCommands() {
        #expect(Commands.start(backlog: true, resume: [:]) == "start")
        #expect(Commands.start(backlog: false, resume: [:]) == "live")
        #expect(Commands.start(backlog: false, resume: [.right: 40, .left: 12]) == "start L:12 R:40")
        #expect(Commands.start(backlog: true, resume: [.unibody: 0]) == "start U:0")
    }
}
