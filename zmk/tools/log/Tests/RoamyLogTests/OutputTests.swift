import Testing
@testable import RoamyLog

@Suite struct OutputTests {
    @Test func formatsARecordOnALine() {
        #expect(Output.line(fixtureRecords[0]) == "L 00:00:01.234 inf zmk: Welcome to ZMK!")
        #expect(Output.line(fixtureRecords[3]) == "L 00:00:05.000 err roamy_log: räksmörgås ✓")
        #expect(Output.line(fixtureRecords[4]) == "U 00:00:00.000 raw ")
        #expect(Output.line(fixtureRecords[2]).hasPrefix("R 01:02:03.004 wrn bt_hci_core: abcdef"))
        #expect(Output.line(fixtureRecords[2]).hasSuffix("xyzabcdefghij [cut]"))
    }

    @Test func indentsTheTextsLaterLines() {
        let multiline = LogRecord(half: .right, seq: 0, backlog: false, uptimeMs: 360_000_000, level: 4,
                                  source: "zmk", text: "one\r\ntwo\nthree")
        #expect(Output.line(multiline) == "R 100:00:00.000 dbg zmk: one\n    two\n    three")
    }

    @Test func formatsEvents() {
        #expect(Output.line(.dropped(.left, count: 1)) == "L -- 1 record dropped --")
        #expect(Output.line(.dropped(.right, count: 3)) == "R -- 3 records dropped --")
        #expect(Output.line(.restarted(.right)) == "R -- restarted --")
        #expect(Output.line(.backlogEnded(.left)) == "L -- end of backlog; live from here --")
    }

    @Test func parsesTheSourceList() throws {
        let note = LogRecord(half: .left, seq: 9, backlog: false, uptimeMs: 1, level: 3, source: "roamy_log",
                             text: "sources 1-3 of 40: zmk=inf bt_hci_core=dbg roamy_log=inf")
        let list = try #require(SourceList(note))
        #expect(list.first == 1 && list.last == 3 && list.total == 40)
        #expect(list.entries.map(\.name) == ["zmk", "bt_hci_core", "roamy_log"])
        #expect(list.entries.map(\.level) == ["inf", "dbg", "inf"])
        #expect(SourceList(fixtureRecords[3]) == nil)
        #expect(SourceList(LogRecord(half: .left, seq: 0, backlog: false, uptimeMs: 0, level: 3,
                                     source: "zmk", text: "sources 1-3 of 40: zmk=inf")) == nil)
    }
}
