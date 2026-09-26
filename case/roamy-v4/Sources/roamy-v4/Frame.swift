import Foundation
import Cadova

/// Module frame: x across the column from the socket mouth plane, y along the column from the outer face of
/// the SW1 end wall, z up from the board's back toward its front. Board-frame numbers (KiCad, seen from the
/// front, y pointing down) map through `bx`/`by`; y is reversed, since taking it as is with the front up
/// mirrors the board.
enum Frame {
    static func bx(_ x: Double) -> Double { P.wall + P.boardClearance + (x - P.boardOriginX) }
    static func by(_ y: Double) -> Double { P.endWall + P.boardClearance + (P.boardOriginY + P.boardLength - y) }
    static var pocketX0: Double { P.wall }
    static var pocketX1: Double { P.wall + P.pocketWidth }
    static var pocketY0: Double { P.endWall }
    static var pocketY1: Double { P.endWall + P.pocketLength }
    static var headerEdgeX: Double { bx(P.boardOriginX + P.boardWidth) }
}

/// The joint in the x-z section. The tilted header pins fix where the neighbour sits: its socket bore lies
/// on this module's pin line, its mouth at `mouthPastHeaderEdge`. That pose is one rotation by the joint
/// angle about `centre`, about 150 mm below the board. For the outer edges of neighbours to meet, the
/// module's outline is a keystone about that centre: both faces radial, top and bottom perpendicular to the
/// bisector. The bisector leans `skew` from the board normal, since the tilted header puts the neighbour
/// 1.1 mm lower at the seam than a straight one would.
enum Joint {
    static let c = cos(P.jointAngle.radians), s = sin(P.jointAngle.radians)

    /// Header tilt pivot: inboard end of the header pads, on the board's back.
    static var pivot: Vector2D { Vector2D(Frame.headerEdgeX - P.headerPadInboardEnd, 0) }

    /// Rotation by the joint angle about the header pivot; +x goes down.
    static func tilt(_ p: Vector2D) -> Vector2D {
        let d = p - pivot
        return pivot + Vector2D(c * d.x + s * d.y, -s * d.x + c * d.y)
    }

    /// A point of the neighbour, in this module's frame.
    static func neighbour(_ p: Vector2D) -> Vector2D {
        tilt(p + Vector2D(Frame.headerEdgeX + P.mouthPastHeaderEdge, 0))
    }

    static var neighbourTransform: Transform3D {
        let t = neighbour(Vector2D(0, 0))
        return Transform3D([[c, 0, s, t.x], [0, 1, 0, 0], [-s, 0, c, t.y], [0, 0, 0, 1]])
    }

    /// Fixed point of `neighbour`: the arc's centre.
    static var centre: Vector2D {
        // (I - R)(p - F) = R m, R the tilt rotation, m the mouth offset along x
        let m = Frame.headerEdgeX + P.mouthPastHeaderEdge
        let rm = Vector2D(c * m, -s * m)
        let a = 1 - c, b = -s          // I - R = [[a, b], [-b, a]]
        let det = a * a + b * b
        return pivot + Vector2D((a * rm.x - b * rm.y) / det, (b * rm.x + a * rm.y) / det)
    }

    static var mouth: Vector2D { Vector2D(0, P.pinAxisZ) }
    static var socketFace: Vector2D { (mouth - centre).normalized }
    static var headerFace: Vector2D { (neighbour(mouth) - centre).normalized }
    /// Outward normal of the top, and of every other level.
    static var up: Vector2D { (socketFace + headerFace).normalized }
    static var along: Vector2D { Vector2D(up.y, -up.x) }
    static var skew: Angle { atan2(up.x, up.y) }

    static func level(_ p: Vector2D) -> Double { (p - centre) ⋅ up }
    /// Where a face meets a level.
    static func corner(_ face: Vector2D, _ level: Double) -> Vector2D { centre + face * (level / (face ⋅ up)) }
    /// z of a level at x.
    static func z(level: Double, x: Double) -> Double { centre.y + (level - (x - centre.x) * up.x) / up.y }

    /// Top: through the plate top at the header-side edge of the switch pockets, so pockets are 0 deep there.
    static var top: Double { level(Vector2D(Frame.bx(P.keyX) + P.switchFlange / 2, P.plateTopZ)) }
    /// Floor top: componentClearance under the lowest component.
    static var rim: Double {
        let header = [(0.0, P.bodyFloat + P.bodyHeight), (P.headerBodyDepth, P.bodyFloat + P.bodyHeight)]
            .map { tilt(Vector2D(Frame.headerEdgeX - P.headerPadInboardEnd + P.headerTailReach + $0.0, -$0.1)) }
        let socket = [0.0, P.socketDepth].map { Vector2D($0, -(P.bodyFloat + P.bodyHeight)) }
        let hotswap = [Frame.bx(75.5), Frame.bx(84.5)].map { Vector2D($0, -1.8) }
        return (header + socket + hotswap).map(level).min()! - P.componentClearance
    }
    static var bottom: Double { rim - P.floorThickness }

    /// Tilted header body corners, for windows and the mock-up.
    static var headerBodyTop: Double {
        [0.0, P.headerBodyDepth].map { tilt(Vector2D(Frame.headerEdgeX - P.headerPadInboardEnd + P.headerTailReach + $0, -P.bodyFloat)).y }.max()!
    }
}

extension Vector2D {
    static func ⋅ (a: Vector2D, b: Vector2D) -> Double { a.x * b.x + a.y * b.y }
}

/// A prism along y over the whole module length, from an x-z section.
func alongColumn(_ points: [Vector2D], from y0: Double = 0, length: Double = P.outerLength) -> any Geometry3D {
    Polygon(points).extruded(height: length)
        .transformed(Transform3D([[1, 0, 0, 0], [0, 0, -1, y0 + length], [0, 1, 0, 0], [0, 0, 0, 1]]))
}

/// A prism along x from a y-z section.
func acrossColumn(_ points: [Vector2D], from x0: Double, to x1: Double) -> any Geometry3D {
    Polygon(points).extruded(height: x1 - x0)
        .transformed(Transform3D([[0, 0, 1, x0], [1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 0, 1]]))
}

/// Section between two levels, bounded by the two faces.
func keystone(from lower: Double, to upper: Double) -> [Vector2D] {
    [Joint.corner(Joint.socketFace, lower), Joint.corner(Joint.headerFace, lower),
     Joint.corner(Joint.headerFace, upper), Joint.corner(Joint.socketFace, upper)]
}
