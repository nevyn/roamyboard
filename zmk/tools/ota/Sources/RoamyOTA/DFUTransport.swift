import Foundation

/// A connection to a bootloader's legacy DFU service, which `LegacyDFUUpdate` drives.
///
/// Every method that waits must end promptly when its task is cancelled, by throwing
/// `CancellationError`, and must throw `DFUTransportError.disconnected` once the link is gone.
public protocol DFUTransport: Sendable {
    /// Reads the DFU revision characteristic.
    /// - Returns: The revision, or nil if the service has no revision characteristic.
    func readRevision() async throws -> DFURevision?

    /// Writes `value` to the control point with response.
    /// - Returns: When the peripheral has acknowledged the write.
    /// - Throws: `DFUTransportError.writeFailed` if the peripheral refused the write,
    ///   `.droppedBeforeAcknowledgement` if the link dropped after the write went out, and
    ///   `.disconnected` if the link was gone before the write.
    func writeControlPoint(_ value: [UInt8]) async throws

    /// Returns once the link can take a write without response, so that `writePacket` does not
    /// overrun the outgoing queue.
    func waitUntilReadyToSendPacket() async throws

    /// Queues `value` on the packet characteristic as a write without response. Call
    /// `waitUntilReadyToSendPacket()` first.
    func writePacket(_ value: [UInt8]) async throws

    /// The next notification from the control point, in arrival order. Notifications that
    /// arrive while nobody waits are kept.
    func nextNotification() async throws -> [UInt8]

    /// The most bytes that one `writePacket` may carry on this connection (ATT MTU minus 3).
    func maximumPacketLength() async -> Int
}

public enum DFUTransportError: Error, Equatable, CustomStringConvertible {
    /// The link dropped; the associated text is the system's reason, if it gave one.
    case disconnected(String?)
    /// The peripheral refused a write with response; the associated text is the system's reason.
    case writeFailed(String)
    /// The link dropped after a write with response went out and before the peripheral
    /// acknowledged it; the associated text is the system's reason, if it gave one.
    case droppedBeforeAcknowledgement(String?)
    /// Any other failure of the link, described by the associated text.
    case failed(String)

    public var description: String {
        switch self {
        case .disconnected(let reason?): "the connection dropped (\(reason))"
        case .disconnected(nil): "the connection dropped"
        case .writeFailed(let reason): "the half refused a control point write (\(reason))"
        case .droppedBeforeAcknowledgement(let reason?): "the connection dropped before the half acknowledged a control point write (\(reason))"
        case .droppedBeforeAcknowledgement(nil): "the connection dropped before the half acknowledged a control point write"
        case .failed(let reason): reason
        }
    }
}
