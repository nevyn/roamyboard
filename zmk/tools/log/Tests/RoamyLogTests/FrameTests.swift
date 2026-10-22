import Foundation
import Testing
@testable import RoamyLog

/// The notifications in Fixtures/log_frames.txt, which zmk/tests/log_test.c writes with the
/// firmware's own encoder, grouped by payload size.
func fixtureFrames() throws -> [(payload: Int, frames: [[UInt8]])] {
    let url = URL(fileURLWithPath: #filePath).deletingLastPathComponent()
        .appendingPathComponent("Fixtures/log_frames.txt")
    var groups: [(payload: Int, frames: [[UInt8]])] = []
    for line in try String(contentsOf: url, encoding: .utf8).split(separator: "\n") where !line.hasPrefix("#") {
        let parts = line.split(separator: " ")
        let payload = try #require(Int(parts[0]))
        let hex = Array(parts[1].utf8)
        let bytes = stride(from: 0, to: hex.count, by: 2).map { UInt8(String(decoding: hex[$0..<($0 + 2)], as: UTF8.self), radix: 16)! }
        if groups.last?.payload != payload { groups.append((payload, [])) }
        groups[groups.count - 1].frames.append(bytes)
    }
    return groups
}

/// The records that log_test.c's print_fixture() encodes, in order.
let fixtureRecords: [LogRecord] = {
    let alphabet = String((0..<192).map { Character(UnicodeScalar(UInt8(97 + $0 % 26))) })
    return [
        LogRecord(half: .left, seq: 0, backlog: true, uptimeMs: 1234, level: 3, source: "zmk", text: "Welcome to ZMK!"),
        LogRecord(half: .left, seq: 1, backlog: true, uptimeMs: 1300, level: 4, source: "zmk",
                  text: "kscan_165_chain_poll: 7 key modules"),
        LogRecord(half: .right, seq: 7, backlog: false, uptimeMs: 3_723_004, level: 2, truncated: true,
                  source: "bt_hci_core", text: alphabet),
        LogRecord(half: .left, seq: 4, backlog: false, uptimeMs: 5000, level: 1, source: "roamy_log", text: "räksmörgås ✓"),
        LogRecord(half: .unibody, seq: 0, backlog: false, uptimeMs: 0, level: 0, source: "", text: ""),
    ]
}()

@Suite struct FrameTests {
    @Test func decodesTheFirmwaresFramesAtEveryPayloadSize() throws {
        let groups = try fixtureFrames()
        #expect(groups.map(\.payload) == [20, 62, 244])
        for group in groups {
            var reassembler = Reassembler()
            var decoded: [LogRecord] = []
            for frame in group.frames {
                #expect(frame.count <= group.payload)
                if let record = try reassembler.feed(frame) { decoded.append(record) }
            }
            #expect(decoded == fixtureRecords, "payload \(group.payload)")
            #expect(reassembler.incompleteRecords == 0)
        }
    }

    @Test func cutsRecordsIntoFragmentsOfThePayloadSize() throws {
        let groups = try fixtureFrames()
        // Bodies of 24, 44, 209, 32 and 6 bytes, at 14, 56 and 238 bytes per fragment.
        #expect(groups.map(\.frames.count) == [2 + 4 + 15 + 3 + 1, 1 + 1 + 4 + 1 + 1, 5])
    }

    @Test func aMissingFragmentDropsItsRecordOnly() throws {
        let frames = try fixtureFrames()[0].frames
        var reassembler = Reassembler()
        // The third record's fragments are frames 6 to 20; leave out frame 8.
        var decoded: [LogRecord] = []
        var errors: [FrameError] = []
        for (index, frame) in frames.enumerated() where index != 8 {
            do {
                if let record = try reassembler.feed(frame) { decoded.append(record) }
            } catch {
                errors.append(error)
            }
        }
        #expect(decoded.map(\.seq) == [0, 1, 4, 0])
        #expect(errors.first == .outOfOrder(half: .right, seq: 7, index: 3))
        #expect(errors.count == 12)
    }

    @Test func aRecordCutShortByTheNextCounts() throws {
        let frames = try fixtureFrames()[0].frames
        var reassembler = Reassembler()
        #expect(try reassembler.feed(frames[0]) == nil)
        #expect(try reassembler.feed(frames[2]) == nil)
        #expect(reassembler.incompleteRecords == 1)
        reassembler.reset()
        #expect(reassembler.incompleteRecords == 2)
    }

    @Test func rejectsShortFramesAndOtherVersions() throws {
        var reassembler = Reassembler()
        #expect(throws: FrameError.tooShort(length: 5)) { try reassembler.feed([0x11, 0, 0, 0, 0]) }
        var frame = try fixtureFrames()[2].frames[0]
        frame[0] = frame[0] & 0x0F | 0x20
        #expect(throws: FrameError.unsupportedVersion(2)) { try reassembler.feed(frame) }
    }

    @Test func rejectsABodyShorterThanItsSourceName() {
        var reassembler = Reassembler()
        let frame: [UInt8] = [0x15, 0, 1, 0, 0, 0, 0, 0, 0, 0, 3, 9, 0x7A]
        #expect(throws: FrameError.malformedBody(half: .left, seq: 1)) { try reassembler.feed(frame) }
    }
}
