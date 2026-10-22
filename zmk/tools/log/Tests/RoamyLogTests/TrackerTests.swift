import Testing
@testable import RoamyLog

func record(_ half: Half, _ seq: UInt32, at uptime: UInt32, backlog: Bool = false) -> LogRecord {
    LogRecord(half: half, seq: seq, backlog: backlog, uptimeMs: uptime, level: 3, source: "zmk", text: "")
}

@Suite struct SequenceTrackerTests {
    @Test func reportsGapsPerHalf() {
        var tracker = SequenceTracker()
        #expect(tracker.observe(record(.left, 10, at: 100)) == [])
        #expect(tracker.observe(record(.right, 3, at: 50)) == [])
        #expect(tracker.observe(record(.left, 11, at: 110)) == [])
        #expect(tracker.observe(record(.left, 15, at: 120)) == [.dropped(.left, count: 3)])
        #expect(tracker.observe(record(.right, 5, at: 60)) == [.dropped(.right, count: 1)])
        #expect(tracker.droppedTotal == 4)
        #expect(tracker.resumePoints == [.left: 16, .right: 6])
    }

    @Test func theFixturesStreamHasItsGapAndItsBacklogsEnd() {
        var tracker = SequenceTracker()
        let events = fixtureRecords.map { tracker.observe($0) }
        #expect(events == [[], [], [], [.dropped(.left, count: 2), .backlogEnded(.left)], []])
    }

    @Test func aLowerSequenceNumberOrUptimeIsARestart() {
        var tracker = SequenceTracker()
        _ = tracker.observe(record(.right, 40, at: 9000))
        #expect(tracker.observe(record(.right, 0, at: 20, backlog: true)) == [.restarted(.right)])
        #expect(tracker.observe(record(.right, 1, at: 30, backlog: true)) == [])
        // The sequence number goes on, but the uptime went back: it restarted and logged a lot.
        _ = tracker.observe(record(.right, 2, at: 40))
        #expect(tracker.observe(record(.right, 60, at: 10)) == [.restarted(.right)])
        #expect(tracker.droppedTotal == 0)
    }

    @Test func theBacklogEndsOncePerHalf() {
        var tracker = SequenceTracker()
        _ = tracker.observe(record(.left, 0, at: 1, backlog: true))
        #expect(tracker.observe(record(.left, 1, at: 2)) == [.backlogEnded(.left)])
        #expect(tracker.observe(record(.left, 2, at: 3)) == [])
        // A half whose first record is live had no backlog to end.
        #expect(tracker.observe(record(.right, 0, at: 1)) == [])
        #expect(tracker.observe(record(.right, 1, at: 2)) == [])
    }
}
