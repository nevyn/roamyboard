/// Follows each half's sequence numbers, to report records that never arrived, restarts, and
/// where a half's backlog ends.
public struct SequenceTracker: Sendable {
    /// Something to tell the reader about a half, besides its records.
    public enum Event: Equatable, Sendable {
        /// count records between the previous record and this one never arrived: the half's
        /// backlog or the left half's relay buffer dropped them before they could be sent.
        case dropped(Half, count: UInt32)
        /// The half started again: its sequence number or its uptime went backwards.
        case restarted(Half)
        /// The half's records from here on are live; the ones before were its backlog.
        case backlogEnded(Half)
    }

    private struct State {
        var seq: UInt32
        var uptimeMs: UInt32
        var sawBacklog: Bool
        var live: Bool
    }

    private var states: [Half: State] = [:]
    /// Records that went missing, over all halves.
    public private(set) var droppedTotal: UInt64 = 0

    public init() {}

    /// Notes a record and returns what to report before it.
    ///
    /// A record whose sequence number is not above the previous one's, or whose uptime is below
    /// it, starts the half over: the firmware never sends a record twice. An uptime that wraps
    /// after 49.7 days therefore reads as a restart.
    public mutating func observe(_ record: LogRecord) -> [Event] {
        var events: [Event] = []
        guard var state = states[record.half] else {
            states[record.half] = State(seq: record.seq, uptimeMs: record.uptimeMs,
                                        sawBacklog: record.backlog, live: !record.backlog)
            return []
        }
        if record.seq <= state.seq || record.uptimeMs < state.uptimeMs {
            events.append(.restarted(record.half))
            state = State(seq: record.seq, uptimeMs: record.uptimeMs, sawBacklog: record.backlog,
                          live: !record.backlog)
            states[record.half] = state
            return events
        }
        if record.seq > state.seq &+ 1 {
            let missing = record.seq - state.seq - 1
            droppedTotal += UInt64(missing)
            events.append(.dropped(record.half, count: missing))
        }
        if !record.backlog && !state.live {
            state.live = true
            if state.sawBacklog { events.append(.backlogEnded(record.half)) }
        }
        state.sawBacklog = state.sawBacklog || record.backlog
        state.seq = record.seq
        state.uptimeMs = record.uptimeMs
        states[record.half] = state
        return events
    }

    /// The sequence number after each half's latest record, to resume a session from.
    public var resumePoints: [Half: UInt32] {
        states.mapValues { $0.seq &+ 1 }
    }
}
