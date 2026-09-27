import Foundation
import Cadova

/// Harness attachment on a module's free face, as on v3: a ring standing out from the face at one column end,
/// the strap threaded through its slot. Its top lies in the module's top plane, so it prints standing on the bed
/// in the print pose; its bottom continues the module's bottom, turned down by `droop` about the face's bottom edge
/// to follow the leg.
struct Lug: Geometry3D {
    /// x where the free face, which stands perpendicular to the top, crosses the board's back (z = 0).
    let faceX: Double
    /// +1 when the lug reaches toward the header side (+`Joint.along`), -1 toward the socket side.
    let outward: Double
    let end: ColumnEnd
    var droop: Angle = 0°
    /// Level of the module's top, which the lug's top lies in.
    var top = Joint.top
    /// Level from which the lug's root, inside the body, starts: the rim when a separate floor fills the body below it.
    var rootFrom = Joint.bottom

    var body: any Geometry3D {
        let reach = P.lugReach, length = P.lugLength, wall = P.lugWall, height = top - Joint.bottom
        let u = { (a: Double) in a * outward }
        let ring = Rectangle(x: reach + 1, y: length).translated(x: -1)
            .rounded(outsideRadius: P.lugRounding)
            .subtracting { Rectangle(x: reach - 2 * wall, y: length - 2 * wall).translated(x: wall, y: wall) }
            .scaled(x: outward)
        // u-w section: top flat, bottom drooping from the face's bottom edge
        let sag = (reach + 1) * tan(droop.radians)
        let root = rootFrom - Joint.bottom
        let profile = [Vector2D(u(0), 0), Vector2D(u(reach + 1), -sag), Vector2D(u(reach + 1), height), Vector2D(u(-2), height),
                       Vector2D(u(-2), root)] + (root > 0 ? [Vector2D(u(0), root)] : [])
        ring.extruded(height: height + sag + 2).translated(z: -sag - 1)
            .intersecting {
                Polygon(outward > 0 ? profile : profile.reversed()).extruded(height: length).rotated(x: 90°).translated(y: length)
            }
            .transformed(Joint.levelFrame(x: faceX, y: end == .sw1 ? 0 : P.outerLength - length, level: Joint.bottom))
    }
}
