import Foundation
import Cadova

/// Module frame: x across the column from the socket wall's outer face, y along the column from
/// the outer face of the SW5 end wall, z up from the underside of the floor. Board-frame numbers
/// from Electronics.md map through `bx` and `by`.
enum Frame {
    static func bx(_ x: Double) -> Double { P.wall + P.boardClearance + (x - P.boardOriginX) }
    static func by(_ y: Double) -> Double { P.endWall + P.boardClearance + (y - P.boardOriginY) }
    static var pocketX0: Double { P.wall }
    static var pocketX1: Double { P.wall + P.pocketWidth }
    static var pocketY0: Double { P.endWall }
    static var pocketY1: Double { P.endWall + P.pocketLength }

    /// Outer face of the header wall at height z. The face leans by the joint angle about the
    /// pin axis, so a neighbour's flat socket wall sits flush against it.
    static func headerFaceX(z: Double) -> Double {
        P.outerWidth + (z - P.pinAxisZ) * tan(P.jointAngle.radians)
    }

    /// The neighbouring module's frame, expressed in this one: rotated by the joint angle about the
    /// line where the header face meets the pin axis. Header pins and hooks are built in the
    /// neighbour's frame and placed with this.
    static var neighbour: Transform3D {
        Transform3D.translation(z: -P.pinAxisZ)
            .rotated(y: P.jointAngle)
            .translated(x: P.outerWidth, z: P.pinAxisZ)
    }
}

/// The module's outer prism: rectangular in plan, the header side leaning by the joint angle.
struct OuterPrism: Geometry3D {
    var body: any Geometry3D {
        let h = P.height
        Polygon([
            [0, 0], [Frame.headerFaceX(z: 0), 0], [Frame.headerFaceX(z: h), h], [0, h],
        ])
        .extruded(height: P.outerLength)
        .rotated(x: 90°)
        .translated(y: P.outerLength)
    }
}
