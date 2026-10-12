import Foundation

/// A half in OTA mode that `HalfUpdate` connects to, and connects to again after a failed attempt.
///
/// Every method that waits must end promptly when its task is cancelled, by throwing
/// `CancellationError`.
public protocol DFUPeripheral: Sendable {
    associatedtype Transport: DFUTransport

    /// Connects to the half and prepares its DFU service.
    /// - Throws: `TimeoutError` after `timeout` and `DFUTransportError` when the link fails, which
    ///   `HalfUpdate` may retry; any other error when the half cannot be updated at all, such as
    ///   a missing DFU service. The connection attempt is cancelled in every case.
    func connect(within timeout: Duration) async throws -> Transport

    /// Returns at the half's next advertisement; advertisements from before the call do not
    /// count. A bootloader in OTA mode advertises whenever no central is connected to it.
    /// - Throws: `CancellationError` when the task is cancelled, or an error that ends the wait
    ///   for good, such as Bluetooth turning off.
    func waitForAdvertisement() async throws

    /// Drops the connection that `connect` returned; pending waits on `transport` throw
    /// `DFUTransportError.disconnected`.
    func disconnect(_ transport: Transport)
}

/// When and how `HalfUpdate` connects to a half again after a failed attempt.
public struct RetryPolicy: Sendable {
    /// How many times a half's update may start over, on a new connection, after a retryable
    /// failure (`HalfUpdateFailure.Reason.isRetryable`). 0 never connects again. Default 2.
    public var retries = 2
    /// How long to wait for the half to advertise again before each new connection. Default 120 s.
    public var readvertiseTimeout: Duration = .seconds(120)
    /// How long each connection may take to come up, service discovery included. Default 20 s.
    public var connectTimeout: Duration = .seconds(20)

    public init() {}
}

/// Progress reported by `HalfUpdate.run`.
public enum HalfUpdateEvent: Equatable, Sendable {
    /// Attempt `attempt`, counted from 1, connects to the half.
    case connecting(attempt: Int)
    /// Progress of the DFU sequence in the current attempt.
    case dfu(DFUEvent)
    /// The previous attempt failed for `reason`; attempt `attempt` of at most `limit` follows once
    /// the half advertises again.
    case retrying(attempt: Int, limit: Int, reason: HalfUpdateFailure.Reason)
    /// Start DFU in attempt `attempt` found an interrupted transfer and reset the bootloader;
    /// the same attempt starts over once the half advertises again.
    case resetInterruptedTransfer(attempt: Int)
    /// Waiting for the half to advertise again, at most `RetryPolicy.readvertiseTimeout`.
    case waitingForAdvertisement
}

/// Why a half's update failed for good, after how many attempts.
public struct HalfUpdateFailure: Error, Equatable, CustomStringConvertible {
    public enum Reason: Error, Equatable, Sendable, CustomStringConvertible {
        /// The DFU sequence failed.
        case dfu(DFUFailure)
        /// The connection did not come up: the text says why. Retryable.
        case connect(String)
        /// The half or Bluetooth failed in a way that a new connection does not fix: the text
        /// says what and in which step.
        case peripheral(String)
        /// The half did not advertise again within this duration.
        case notAdvertising(Duration)
        case cancelled

        /// Whether a new connection may get further: see `DFUFailure.isRetryable`. A failed
        /// connection is retryable; a half that stopped advertising is not.
        public var isRetryable: Bool {
            switch self {
            case .dfu(let failure): failure.isRetryable
            case .connect: true
            case .peripheral, .notAdvertising, .cancelled: false
            }
        }

        /// What went wrong, without advice.
        public var summary: String {
            switch self {
            case .dfu(let failure): failure.summary
            case .connect(let reason): "connecting failed: \(reason)"
            case .peripheral(let reason): reason
            case .notAdvertising(let duration): "the half did not advertise as AdaDFU again within \(duration.formatted(.units(allowed: [.seconds])))"
            case .cancelled: "cancelled"
            }
        }

        public var description: String {
            if case .dfu(let failure) = self { return failure.description }
            return "\(summary)."
        }
    }

    public let reason: Reason
    /// Attempts made, the failed one included.
    public let attempts: Int
    /// Whether the half has lost its old firmware: true once any attempt got past Start DFU, or
    /// found an interrupted transfer.
    public let firmwareErased: Bool
    /// The DFU zip's file name, which the advice names.
    public let zipName: String

    public init(reason: Reason, attempts: Int, firmwareErased: Bool, zipName: String) {
        self.reason = reason
        self.attempts = attempts
        self.firmwareErased = firmwareErased
        self.zipName = zipName
    }

    public var description: String {
        let tries = attempts > 1 ? " roamy-ota gave up after \(attempts) attempts." : ""
        return "\(reason)\(tries)\(advice.map { " \($0)" } ?? "")"
    }

    private var advice: String? {
        let flash = "double-tap its reset button and flash the UF2 over USB (docs/firmware.md, Flashing)"
        switch (reason, firmwareErased) {
        case (.notAdvertising, true):
            return "The half has no firmware now: \(flash), or run roamy-ota again with \(zipName) once it advertises as AdaDFU."
        case (.notAdvertising, false):
            return "Press OTA on that half and run roamy-ota again with \(zipName), or \(flash)."
        case (_, true):
            return "The half has no firmware now and stays in OTA mode: run roamy-ota again with \(zipName), or \(flash)."
        case (.connect, false), (.peripheral, false):
            return "The half stays in OTA mode; run roamy-ota again with \(zipName)."
        default:
            return nil
        }
    }
}

/// Updates one half with `LegacyDFUUpdate`, and starts the update over on a new connection when
/// an attempt fails in a way that `HalfUpdateFailure.Reason.isRetryable` allows. See
/// docs/firmware.md, "From a Mac".
public struct HalfUpdate: Sendable {
    public let update: LegacyDFUUpdate
    public let policy: RetryPolicy

    public init(update: LegacyDFUUpdate, policy: RetryPolicy) {
        self.update = update
        self.policy = policy
    }

    /// Connects to `peripheral` and runs the update, at most `policy.retries + 1` times.
    ///
    /// Before each new attempt, it waits for the half to advertise again. When Start DFU finds an
    /// interrupted transfer, `LegacyDFUUpdate` resets the bootloader, and the same attempt starts
    /// over once the half advertises again; a second interrupted transfer in one attempt fails
    /// that attempt. With `policy.retries` 0, it never connects again, as `LegacyDFUUpdate` alone.
    /// - Parameter events: Called with each step, and with `DFUEvent`s from each attempt.
    /// - Throws: `HalfUpdateFailure` with the last attempt's reason; `.cancelled` if the task is
    ///   cancelled while it waits for the half or connects.
    public func run<Peripheral: DFUPeripheral>(on peripheral: Peripheral, events: @Sendable (HalfUpdateEvent) -> Void = { _ in }) async throws(HalfUpdateFailure) {
        var attempt = 1
        var erased = false
        var resetInThisAttempt = false
        var waitFirst = false
        while true {
            let reason: HalfUpdateFailure.Reason
            do {
                try await self.attempt(attempt, on: peripheral, waitFirst: waitFirst, events: events)
                return
            } catch {
                reason = error
            }
            if case .dfu(let failure) = reason, failure.firmwareErased { erased = true }
            waitFirst = true
            if policy.retries > 0, !resetInThisAttempt, case .dfu(let failure) = reason, failure.reason == .interruptedTransferReset {
                resetInThisAttempt = true
                events(.resetInterruptedTransfer(attempt: attempt))
                continue
            }
            if reason.isRetryable, attempt <= policy.retries {
                attempt += 1
                resetInThisAttempt = false
                events(.retrying(attempt: attempt, limit: policy.retries + 1, reason: reason))
                continue
            }
            throw HalfUpdateFailure(reason: reason, attempts: attempt, firmwareErased: erased, zipName: update.zip.name)
        }
    }

    private func attempt<Peripheral: DFUPeripheral>(_ number: Int, on peripheral: Peripheral, waitFirst: Bool, events: @Sendable (HalfUpdateEvent) -> Void) async throws(HalfUpdateFailure.Reason) {
        if waitFirst {
            events(.waitingForAdvertisement)
            do {
                try await withTimeout(policy.readvertiseTimeout) { try await peripheral.waitForAdvertisement() }
            } catch is TimeoutError {
                throw .notAdvertising(policy.readvertiseTimeout)
            } catch is CancellationError {
                throw .cancelled
            } catch {
                throw .peripheral("waiting for the half to advertise as AdaDFU failed: \(error)")
            }
        }

        events(.connecting(attempt: number))
        let transport: Peripheral.Transport
        do {
            transport = try await peripheral.connect(within: policy.connectTimeout)
        } catch let error as TimeoutError {
            throw .connect("no connection within \(error.duration.formatted(.units(allowed: [.seconds])))")
        } catch let error as DFUTransportError {
            throw .connect("\(error)")
        } catch is CancellationError {
            throw .cancelled
        } catch {
            throw .peripheral("connecting failed: \(error)")
        }
        defer { peripheral.disconnect(transport) }

        do {
            try await update.run(over: transport) { events(.dfu($0)) }
        } catch {
            throw .dfu(error)
        }
    }
}
