import Foundation
import Cadova

/// Top shell of a key module: plate with the switch openings and pockets, walls with bottom-open connector
/// slots, ledges that stop the board from above, and joint hardware on both sides (`HeaderSideJoint`,
/// `SocketSideJoint`). Open at the bottom, so the board drops in from below; `KeyModuleFloor` closes it.
struct KeyModuleShell: Geometry3D {
    static let joints: [any JointSide] = [HeaderSideJoint(), SocketSideJoint()]
    var body: any Geometry3D {
        alongColumn(keystone(from: Joint.rim, to: Joint.top))
            .subtracting {
                // cavity under the plate
                Box(x: P.pocketWidth, y: P.pocketLength, z: P.ceilingZ + 20)
                    .translated(x: Frame.pocketX0, y: Frame.pocketY0, z: -20)
                for y in P.keyYs {
                    let at = Vector3D(Frame.bx(P.keyX), Frame.by(y), 0)
                    Box(x: P.switchCutout, y: P.switchCutout, z: 10).aligned(at: .centerXY).translated(at + [0, 0, P.ceilingZ - 1])
                    Box(x: P.switchFlange, y: P.switchFlange, z: 10).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ])
                    // seat chamfer, so the seat hangs at most one extrusion width over the pocket in the print pose
                    let c = P.seatChamfer
                    Box(x: P.switchCutout, y: P.switchCutout, z: 0.01).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ - c])
                        .adding { Box(x: P.switchCutout + 2 * c + 1, y: P.switchCutout + 2 * c + 1, z: 0.01).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ + 0.5]) }
                        .convexHull()
                }
                // socket mouths through the socket wall, open toward the floor, up to the floating body's top
                for r in P.rowYs {
                    Box(x: P.wall + 2, y: P.socketWidth + 2 * P.boardClearance, z: 20 - P.bodyFloat + 0.15)
                        .aligned(at: .centerY)
                        .translated(x: -1, y: Frame.by(r), z: -20)
                }
                // tilted header bodies and pins through the header wall, open toward the floor
                for r in P.rowYs {
                    Box(x: 10, y: P.headerWidth + 2 * P.boardClearance, z: 20 + Joint.headerBodyTop + 0.3)
                        .aligned(at: .centerY)
                        .translated(x: Frame.pocketX1 - 0.5, y: Frame.by(r), z: -20)
                }
                for joint in Self.joints { joint.removed }
                for end in ColumnEnd.allCases { ScrewHoles(end: end) }
                // revision on the SW1 end wall's inner face, below the end ledge, read from the pocket
                engraving("shell \(Revision.label(Revision.shell))", size: 4.0)
                    .scaled(x: -1)
                    .rotated(x: 90°)
                    .translated(x: Frame.pocketX0 + P.pocketWidth / 2, y: Frame.pocketY0 + 0.4, z: -0.6)
            }
            .adding {
                Ledges()
                for joint in Self.joints { joint.added }
            }
    }
}

/// Vertical M2 holes from the rim into the end wall, for the floor.
struct ScrewHoles: Geometry3D {
    let end: ColumnEnd
    var body: any Geometry3D {
        for x in P.screwXs {
            let z0 = Joint.z(level: Joint.rim, x: x)
            Cylinder(diameter: P.screwHole, height: P.screwDepth + 1)
                .translated(x: x, y: end.y(P.screwY), z: z0 - 1)
        }
    }
}

/// Stops the board from moving up: full-width ledges at both ends, a continuous ledge along the header
/// side, and tabs between the switch housings on the socket side, where the housings sit 0.05 mm from the
/// board edge.
struct Ledges: Geometry3D {
    /// z the ledges reach up to, into the plate.
    var ceilingZ = P.ceilingZ
    /// y ranges (module frame) of the header-side ledge; the whole length when nil.
    var headerSide: [(Double, Double)]? = nil
    var body: any Geometry3D {
        let z0 = P.boardTopZ, h = ceilingZ - P.boardTopZ + 0.01
        Box(x: P.pocketWidth, y: P.ledgeLength, z: h).translated(x: Frame.pocketX0, y: Frame.pocketY0, z: z0)
        Box(x: P.pocketWidth, y: P.ledgeLength, z: h).translated(x: Frame.pocketX0, y: Frame.pocketY1 - P.ledgeLength, z: z0)
        for (y0, y1) in headerSide ?? [(Frame.pocketY0, Frame.pocketY1)] {
            Box(x: P.ledgeWidth, y: y1 - y0, z: h).translated(x: Frame.pocketX1 - P.ledgeWidth, y: y0, z: z0)
        }
        for y in P.socketSideTabYs {
            Box(x: P.ledgeWidth, y: 4.0, z: h).aligned(at: .centerY).translated(x: Frame.pocketX0, y: Frame.by(y), z: z0)
        }
    }
}

/// Floor screwed on from below into the end walls' corners, following the keystone. Pillars under the board's
/// bare end margins push it against the ledges; tabs along the walls locate it in the shell's opening.
struct KeyModuleFloor: Geometry3D {
    static func rimAt(_ x: Double) -> Double { Joint.z(level: Joint.rim, x: x) }

    /// The floor's own frame: x along it, z out of its top face, origin on its bottom face at x.
    static func frame(x: Double, y: Double) -> Transform3D {
        let u = Joint.up, t = Joint.along
        let o = Vector2D(x, Joint.z(level: Joint.bottom, x: x))
        return Transform3D([[t.x, 0, u.x, o.x], [0, 1, 0, y], [t.y, 0, u.y, o.y], [0, 0, 0, 1]])
    }

    var body: any Geometry3D {
        let c = P.tabClearance, t = P.tabThickness, h = P.tabHeight
        alongColumn(keystone(from: Joint.bottom, to: Joint.rim))
            .adding {
                for x in P.pillarXs {
                    for y in P.pillarYs {
                        let z0 = Self.rimAt(x) - 0.5
                        Cylinder(diameter: P.pillarDiameter, height: -z0).translated(x: x, y: Frame.by(y), z: z0)
                    }
                }
                // locating tabs along the side walls between the connector rows, and along the end walls
                for (y0, y1) in P.sideTabYs {
                    let ya = Frame.by(y1), yb = Frame.by(y0)
                    for x in [Frame.pocketX0 + c, Frame.pocketX1 - c - t] {
                        Box(x: t, y: yb - ya, z: h + 0.5).translated(x: x, y: ya, z: Self.rimAt(x + t / 2) - 0.5)
                    }
                }
                for (x0, x1) in P.endTabXs {
                    for y in [Frame.pocketY0 + c, Frame.pocketY1 - c - t] {
                        Box(x: x1 - x0, y: t, z: h + 0.5).translated(x: x0, y: y, z: Self.rimAt((x0 + x1) / 2) - 0.5)
                    }
                }
            }
            .subtracting {
                // revision on the inside face, following the floor's tilt
                let o = Joint.corner(Joint.socketFace, Joint.rim) + Joint.along * 10, u = Joint.up, t = Joint.along
                engraving("floor \(Revision.label(Revision.floor))", size: 6.0)
                    .translated(z: -0.6)
                    .transformed(Transform3D([[t.x, 0, u.x, o.x], [0, 1, 0, P.outerLength / 2], [t.y, 0, u.y, o.y], [0, 0, 0, 1]]))
                for end in ColumnEnd.allCases {
                    for x in P.screwXs { Counterbore().transformed(Self.frame(x: x, y: end.y(P.screwY))) }
                }
            }
    }
}

/// Screw clearance and head counterbore through a floor, from its bottom face at z = 0 along z. The head's ceiling
/// is bridged in two layers (a slot, then a square), so the floor prints bottom-down without it drooping into the bore.
struct Counterbore: Geometry3D {
    var body: any Geometry3D {
        let d = P.screwHeadDepth, l = P.layer, s = P.screwClearance
        Cylinder(diameter: P.screwHeadDiameter, height: d + 0.5).translated(z: -0.5)
            .adding {
                Box(x: s, y: P.screwHeadDiameter, z: l).aligned(at: .centerXY).translated(z: d)
                Box(x: s, y: s, z: 2 * l).aligned(at: .centerXY).translated(z: d)
                Cylinder(diameter: s, height: P.floorThickness + 2).translated(z: d + 2 * l)
            }
    }
}

/// Print placements: the shell with its top on the bed, the floor with its bottom on the bed.
enum PrintPose {
    static var shell: Transform3D { topDown(top: Joint.top) }
    /// A part with its top at level `top` on the bed.
    static func topDown(top: Double) -> Transform3D {
        let u = Joint.up, t = Joint.along, o = Joint.centre
        // x' = (p - o)·t, y' = -y, z' = top - (p - o)·up
        return Transform3D([[t.x, 0, t.y, -(o.x * t.x + o.y * t.y)],
                            [0, -1, 0, P.outerLength],
                            [-u.x, 0, -u.y, top + (o.x * u.x + o.y * u.y)],
                            [0, 0, 0, 1]])
    }
    static var floor: Transform3D {
        let u = Joint.up, t = Joint.along, o = Joint.centre
        return Transform3D([[t.x, 0, t.y, -(o.x * t.x + o.y * t.y)],
                            [0, 1, 0, 0],
                            [u.x, 0, u.y, -Joint.bottom - (o.x * u.x + o.y * u.y)],
                            [0, 0, 0, 1]])
    }
}
