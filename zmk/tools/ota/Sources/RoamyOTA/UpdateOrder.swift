/// The order in which `roamy-ota` hands DFU zips to halves, and the line that explains it.
///
/// `roamy-ota` gives the n-th AdaDFU peripheral that it finds the n-th zip. Both halves advertise
/// as "AdaDFU", so the order is the only thing that ties a zip to a half.
///
/// When no AdaDFU advertises as `roamy-ota` starts, every half enters OTA mode when `roamy-ota`
/// asks, and the left half goes last: it runs the keymap and relays the right half's OTA key, so
/// the right half's OTA key works only while the left half still runs its firmware. Zips whose
/// name contains "left" (in any case) move to the end; all other zips keep their relative order.
///
/// When one or more AdaDFU peripherals already advertise, for example a half that is stuck in OTA
/// mode after a failed update, `roamy-ota` finds them first and they get the first zips. The
/// order on the command line is then kept, since only the user knows which half is stuck.
public struct UpdateOrder: Sendable {
    /// The zips in the order that halves get them.
    public let zips: [DFUZip]
    /// A line that tells the user which zip goes where and why, or nil when `zips` is the order
    /// on the command line and no AdaDFU already advertises.
    public let explanation: String?

    /// - Parameters:
    ///   - given: The zips in the order on the command line.
    ///   - alreadyAdvertising: How many AdaDFU peripherals advertised before `roamy-ota` asked
    ///     for the first half.
    public init(_ given: [DFUZip], alreadyAdvertising: Int) {
        let names = given.map(\.name)
        if alreadyAdvertising > 0 {
            zips = given
            let assigned = names.prefix(alreadyAdvertising)
            let found = alreadyAdvertising == 1 ? "An AdaDFU is" : "\(alreadyAdvertising) AdaDFUs are"
            let which = alreadyAdvertising == 1 ? "which half it is" : "which half each one is"
            let gets = alreadyAdvertising == 1
                ? "that half gets \(assigned.first ?? "no zip")"
                : "they get \(Self.list(Array(assigned))) in the order that they were found"
            let advice = switch (alreadyAdvertising, given.count) {
            case (_, ...1): ""
            case (1, _): " Name the stuck half's zip first."
            default: " If a half gets the other half's firmware, flash its UF2 over USB (docs/firmware.md, Flashing)."
            }
            explanation = "\(found) already advertising, and roamy-ota cannot tell \(which), so it keeps your order: \(gets).\(advice)"
            return
        }
        let isLeft = { (zip: DFUZip) in zip.name.lowercased().contains("left") }
        zips = given.filter { !isLeft($0) } + given.filter(isLeft)
        let ordered = zips.map(\.name)
        explanation = ordered == names ? nil
            : "Updating \(ordered.joined(separator: ", ")) in that order: the left half relays the right half's OTA key, so it goes last."
    }

    private static func list(_ names: [String]) -> String {
        names.count <= 2 ? names.joined(separator: " and ") : names.dropLast().joined(separator: ", ") + " and " + names.last!
    }
}
