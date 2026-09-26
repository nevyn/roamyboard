import Foundation
import Cadova

/// Harness attachment on a module's free face, as on v3: a ring standing out from the face at one column end,
/// the strap threaded through its slot. Its top lies in the module's top plane, so it prints standing on the bed
/// in the print pose; its bottom continues the module's bottom, turned down by `droop` about the face's bottom edge
/// to follow the leg.
struct Lug: Geometry3D {
    /// A point on the free face, which stands perpendicular to the top.
    let face: Vector2D
    /// +1 when the lug reaches toward the header side (+`Joint.along`), -1 toward the socket side.
    let outward: Double
    let end: ColumnEnd
    var droop: Angle = 0°

    var body: any Geometry3D {
        let reach = P.lugReach, length = P.lugLength, wall = P.lugWall, height = Joint.top - Joint.bottom
        let u = { (a: Double) in a * outward }
        let ring = Rectangle(x: reach + 1, y: length).translated(x: -1)
            .rounded(outsideRadius: P.lugRounding)
            .subtracting { Rectangle(x: reach - 2 * wall, y: length - 2 * wall).translated(x: wall, y: wall) }
            .scaled(x: outward)
        // u-w section: top flat, bottom drooping from the face's bottom edge
        let sag = (reach + 1) * tan(droop.radians)
        let profile = [Vector2D(u(-2), 0), Vector2D(u(reach + 1), -sag), Vector2D(u(reach + 1), height), Vector2D(u(-2), height)]
        let o = face + Joint.up * (Joint.bottom - Joint.level(face)), t = Joint.along, n = Joint.up
        ring.extruded(height: height + sag + 2).translated(z: -sag - 1)
            .intersecting {
                Polygon(outward > 0 ? profile : profile.reversed()).extruded(height: length).rotated(x: 90°).translated(y: length)
            }
            .transformed(Transform3D([[t.x, 0, n.x, o.x], [0, 1, 0, end == .sw1 ? 0 : P.outerLength - length],
                                      [t.y, 0, n.y, o.y], [0, 0, 0, 1]]))
    }
}
