import Foundation
import Cadova

/// Strap keeper: a flat tube that the harness strap runs through, glued with its plate to the bottom of a floor or the
/// terminator, its slot across the column. It only locates the board on the strap, so glue holds it. Built in its print
/// pose, standing on one end: x across the strap, y from the bridge (against the leg) to the glue plate, z along the strap.
struct StrapKeeper: Geometry3D {
    static var width: Double { P.strapSlot + 2 * P.keeperWall }
    static var height: Double { P.strapBridge + P.strapGap + P.keeperWall }

    /// A thin cut of the slot at z, grown by `grow` on every side.
    static func slot(grow: Double, z: Double) -> any Geometry3D {
        Box(x: P.strapSlot + 2 * grow, y: P.strapGap + 2 * grow, z: 0.01)
            .translated(x: P.keeperWall - grow, y: P.strapBridge - grow, z: z)
    }

    var body: any Geometry3D {
        let s = P.strapSlot, g = P.strapGap, w = P.keeperWall, b = P.strapBridge, l = P.keeperLength, c = P.keeperLeadIn
        Rectangle(x: Self.width, y: Self.height).rounded(outsideRadius: 1.0).extruded(height: l)
            .subtracting {
                Box(x: s, y: g, z: l + 2).translated(x: w, y: b, z: -1)
                Self.slot(grow: c + 0.01, z: -0.01).adding { Self.slot(grow: 0, z: c) }.convexHull()
                Self.slot(grow: c + 0.01, z: l).adding { Self.slot(grow: 0, z: l - c) }.convexHull()
                // revision on the bridge's outer face
                engraving("keeper \(Revision.label(Revision.keeper))", size: 4.0)
                    .rotated(x: 90°)
                    .translated(x: Self.width / 2, y: 0.4, z: l / 2)
            }
    }
}

/// Strap tunnels of a key module or MCU floor: y ranges of the slot at each column end, clear of the corner counterbores.
enum StrapTunnel {
    static var depth: Double { P.strapGap + P.strapBridge }
    static var ys: [(Double, Double)] {
        ColumnEnd.allCases.map { end in
            let a = end.y(P.tunnelInset), b = end.y(P.tunnelInset + P.strapSlot)
            return (min(a, b), max(a, b))
        }
    }
    /// A counterbore's head bore continued down through the thickening, in the screw's frame (origin on the plain floor's bottom face).
    static var headBore: any Geometry3D {
        Cylinder(diameter: P.screwHeadDiameter, height: depth + 1).translated(z: -depth - 0.5)
    }
    /// A screwdriver in the head bore, up to 0.05 mm short of the head; for the checks.
    static var driver: any Geometry3D {
        Cylinder(diameter: 3.2, height: depth + 1 + P.screwHeadDepth - 0.05).translated(z: -depth - 1)
    }

    /// Slot through the thickened floor along the strap, roofed by the plain floor; `shrink` makes a strap that fits it.
    static func slots(shrink: Double = 0) -> any Geometry3D {
        Union {
            for (y0, y1) in ys {
                Box(x: 100, y: y1 - y0 - 2 * shrink, z: P.strapGap - 2 * shrink)
                    .translated(x: -25, y: y0 + shrink, z: -P.strapGap + shrink)
                    .transformed(KeyModuleFloor.frame(x: 0, y: 0))
            }
        }
    }
}
