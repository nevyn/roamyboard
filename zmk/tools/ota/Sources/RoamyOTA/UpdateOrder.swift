/// Puts a split keyboard's left half, the central, after every other half.
///
/// The left half runs the keymap, so it relays the right half's OTA key: the right half can
/// only enter OTA mode while the left half still runs its firmware. Zips whose name contains
/// "left" (in any case) move to the end; all other zips keep their relative order.
/// - Returns: The zips in the order to update them, and whether that order differs from `zips`.
public func updateOrder(_ zips: [DFUZip]) -> (zips: [DFUZip], reordered: Bool) {
    let isLeft = { (zip: DFUZip) in zip.name.lowercased().contains("left") }
    let ordered = zips.filter { !isLeft($0) } + zips.filter(isLeft)
    return (ordered, ordered.map(\.name) != zips.map(\.name))
}
