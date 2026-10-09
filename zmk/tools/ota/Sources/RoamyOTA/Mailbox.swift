import Foundation

/// A queue of values from callbacks (CoreBluetooth delegates) to one async receiver at a time.
///
/// Every `receive()` resumes exactly once: with a value, with the error passed to `close`, or
/// with `CancellationError` when its task is cancelled. That last path is what lets a
/// task group (such as `withTimeout`) cancel a waiting receive instead of hanging on it.
public final class Mailbox<Element: Sendable>: @unchecked Sendable {
    private let lock = NSLock()
    private var buffer: [Element] = []
    private var failure: (any Error)?
    private var waiter: (id: UInt64, continuation: CheckedContinuation<Element, any Error>)?
    private var lastID: UInt64 = 0

    public init() {}

    /// Hands `element` to the waiting receiver, or queues it. Ignored once the mailbox is closed.
    public func post(_ element: Element) {
        lock.lock()
        if let waiter {
            self.waiter = nil
            lock.unlock()
            waiter.continuation.resume(returning: element)
            return
        }
        if failure == nil { buffer.append(element) }
        lock.unlock()
    }

    /// Makes the waiting and every later `receive()` throw `error` once the queued values are
    /// taken. Only the first close counts.
    public func close(throwing error: any Error) {
        lock.lock()
        guard failure == nil else { return lock.unlock() }
        failure = error
        let waiter = self.waiter
        self.waiter = nil
        lock.unlock()
        waiter?.continuation.resume(throwing: error)
    }

    /// Drops the queued values.
    public func drain() {
        lock.withLock { buffer.removeAll() }
    }

    /// The next value, oldest first.
    /// - Throws: The error passed to `close` once no values are queued, or `CancellationError`
    ///   if the calling task is cancelled while it waits.
    /// - Precondition: No other `receive()` is waiting.
    public func receive() async throws -> Element {
        let id = lock.withLock {
            lastID += 1
            return lastID
        }
        return try await withTaskCancellationHandler {
            try await withCheckedThrowingContinuation { continuation in
                lock.lock()
                if !buffer.isEmpty {
                    let element = buffer.removeFirst()
                    lock.unlock()
                    continuation.resume(returning: element)
                } else if let failure {
                    lock.unlock()
                    continuation.resume(throwing: failure)
                } else if Task.isCancelled {
                    lock.unlock()
                    continuation.resume(throwing: CancellationError())
                } else {
                    precondition(waiter == nil, "Mailbox has one receiver at a time")
                    waiter = (id, continuation)
                    lock.unlock()
                }
            }
        } onCancel: {
            lock.lock()
            guard let waiter, waiter.id == id else { return lock.unlock() }
            self.waiter = nil
            lock.unlock()
            waiter.continuation.resume(throwing: CancellationError())
        }
    }
}

/// Thrown by `withTimeout` when the operation did not finish in time.
public struct TimeoutError: Error, Equatable {
    public let duration: Duration
}

/// Runs `operation`, cancelling it after `duration`.
/// - Throws: `TimeoutError` after `duration`, or whatever `operation` throws.
/// - Important: `operation` must end promptly when cancelled; this function waits for it.
public func withTimeout<T: Sendable>(
    _ duration: Duration, _ operation: @escaping @Sendable () async throws -> T
) async throws -> T {
    try await withThrowingTaskGroup(of: T.self) { group in
        group.addTask { try await operation() }
        group.addTask {
            try await Task.sleep(for: duration)
            throw TimeoutError(duration: duration)
        }
        defer { group.cancelAll() }
        return try await group.next()!
    }
}
