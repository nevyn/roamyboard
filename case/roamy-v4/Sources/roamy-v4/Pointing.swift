import Foundation
import Cadova

/// Layout of the pointing module, in the key module frame (`Frame`, `Joint`). It spans two columns: K, a key module's
/// place, with the joint's header side, a key module board and its SW5 switch; and V, the column on K's socket side (K is
/// V's neighbour), with the joint's socket side, three loose sockets and a second SW5 switch. Its faces are two joint
/// angles apart. A flat plateau over both columns, perpendicular to their seam, is the trackpad's overlay; the bottom is
/// flat and parallel to it, so both parts print flat, and the rim follows each column's own.
enum Pointing {
    static let headerJoint = HeaderSideJoint()
    static let socketJoint = SocketSideJoint()

    /// V's frame into the module frame.
    static var v: Transform3D { Joint.neighbourTransform.inverse }
    /// A point of V's x-z section, in the module frame.
    static func fromV(_ p: Vector2D) -> Vector2D {
        let d = p - Joint.pivot, m = Frame.headerEdgeX + P.mouthPastHeaderEdge
        return Joint.pivot + Vector2D(Joint.c * d.x - Joint.s * d.y, Joint.s * d.x + Joint.c * d.y) - Vector2D(m, 0)
    }

    /// Outward normal of the plateau and of every other level of this module: the seam between K and V.
    static var up: Vector2D { Joint.socketFace }
    static var along: Vector2D { Vector2D(up.y, -up.x) }
    static func level(_ p: Vector2D) -> Double { (p - Joint.centre) ⋅ up }
    static var socketFace: Vector2D { (fromV(Joint.mouth) - Joint.centre).normalized }
    static func corner(_ face: Vector2D, _ level: Double) -> Vector2D { Joint.centre + face * (level / (face ⋅ up)) }
    /// Frame at a point of the x-z section: x along the plateau, z along `up`.
    static func frame(at p: Vector2D, y: Double = 0) -> Transform3D {
        Transform3D([[along.x, 0, up.x, p.x], [0, 1, 0, y], [along.y, 0, up.y, p.y], [0, 0, 0, 1]])
    }
    /// Frame on a level, with its origin on the seam, so x = 0 is the middle of the module.
    static func frame(level: Double, y: Double = 0) -> Transform3D { frame(at: Joint.centre + up * level, y: y) }

    // Along the column (module y)
    static var padY1: Double { Frame.by(P.keyYs[0]) - P.switchFlange / 2 - P.padPastSW5 }
    static var padY0: Double { padY1 - P.padLength }

    // Levels
    /// The pad's back: `padOverParts` over K's board front where they come closest, at the board's socket-side edge.
    static var padBack: Double {
        [Frame.bx(P.boardOriginX), Frame.headerEdgeX].map { level(Vector2D($0, P.boardTopZ + P.padOverParts)) }.max()!
    }
    static var padTop: Double { padBack + P.padThickness }
    static var plateau: Double { padTop + P.padSkin }
    /// Through the key modules' bottom edges at both faces, so neighbours' bottoms still meet.
    static var bottom: Double { level(Joint.corner(Joint.headerFace, Joint.bottom)) }

    /// V's rim at its socket face, the seam's, K's at its header face.
    static var rim: [Vector2D] {
        [fromV(Joint.corner(Joint.socketFace, Joint.rim)), Joint.corner(Joint.socketFace, Joint.rim), Joint.corner(Joint.headerFace, Joint.rim)]
    }
    static var shellSection: [Vector2D] { rim + [corner(Joint.headerFace, plateau), corner(socketFace, plateau)] }
    static var floorSection: [Vector2D] { [corner(socketFace, bottom), corner(Joint.headerFace, bottom)] + rim.reversed() }
    /// V's side of the seam.
    static var vSide: [Vector2D] {
        let c = Joint.centre, f = Joint.socketFace * 300, a = along * 100
        return [c - a, c, c + f, c + f - a]
    }

    /// Where the corner screws enter the shell, on the rim: K's at the header side and at the seam, V's at its socket side,
    /// each at a key module's screw position.
    static var screwPoints: [Vector2D] {
        let rimAt = { (x: Double) in Vector2D(x, Joint.z(level: Joint.rim, x: x)) }
        return [rimAt(P.screwXs[1]), rimAt(P.screwXs[0]), fromV(rimAt(P.screwXs[0]))]
    }
    /// Each floor screw's frame: origin on the floor's bottom face, z along `up`, which the pilots follow too.
    static var screwFrames: [Transform3D] {
        ColumnEnd.allCases.flatMap { end in screwPoints.map { frame(at: $0 + up * (bottom - level($0)), y: end.y(P.screwY)) } }
    }

    /// The floor's locating tabs along K's side walls: the key module's, where the walls stand beside the pad.
    static var wallTabYs: [(Double, Double)] {
        P.sideTabYs.map { (Frame.by($0.1), Frame.by($0.0)) }
            .filter { $0.1 < padY0 - P.padClearance || $0.0 > padY1 + P.padClearance }
    }

    /// The pad's pocket under the skin, open toward the floor so the pad goes into the upturned shell.
    static var padPocket: any Geometry3D {
        let c = P.padClearance, depth = padTop - Joint.bottom + 10
        return Box(x: P.padWidth + 2 * c, y: P.padLength + 2 * c, z: depth)
            .translated(x: -P.padWidth / 2 - c, y: padY0 - c, z: -depth)
            .transformed(frame(level: padTop))
    }
}

/// Top of the pointing module: K's pocket, header wall and ledges as in the key module; V hollow from its socket wall to
/// the seam; both SW5 switches in their own columns' plates; the pad's pocket under the plateau's skin. Open at the
/// bottom; `PointingFloor` closes it.
struct PointingShell: Geometry3D {
    var body: any Geometry3D {
        alongColumn(Pointing.shellSection)
            .subtracting {
                Box(x: P.pocketWidth, y: P.pocketLength, z: P.ceilingZ + 20).translated(x: Frame.pocketX0, y: Frame.pocketY0, z: -20)
                for r in P.rowYs {
                    // K's socket wall keeps its mouths, open toward the floor, for the wires to K's J_LEFT pads
                    Box(x: P.wall + 2, y: P.socketWidth + 2 * P.boardClearance, z: 20 - P.bodyFloat + 0.15)
                        .aligned(at: .centerY).translated(x: -1, y: Frame.by(r), z: -20)
                    Box(x: 10, y: P.headerWidth + 2 * P.boardClearance, z: 20 + Joint.headerBodyTop + 0.3)
                        .aligned(at: .centerY).translated(x: Frame.pocketX1 - 0.5, y: Frame.by(r), z: -20)
                }
                Box(x: 30, y: P.pocketLength, z: P.ceilingZ + 20).translated(x: Frame.pocketX0, y: Frame.pocketY0, z: -20)
                    .transformed(Pointing.v)
                    .intersecting { alongColumn(Pointing.vSide) }
                // V's socket mouths, up to the bodies' tops, so the socket wall holds each body down at its mouth
                for r in P.rowYs {
                    Box(x: P.wall + 2, y: P.socketWidth + 2 * P.tabClearance, z: 20).aligned(at: .centerY)
                        .translated(x: -1, y: Frame.by(r), z: -20).transformed(Pointing.v)
                }
                SW5Opening()
                SW5Opening(faceChamfer: true).transformed(Pointing.v)
                Pointing.headerJoint.removed
                Pointing.socketJoint.removed.transformed(Pointing.v)
                for end in ColumnEnd.allCases {
                    for p in Pointing.screwPoints {
                        Cylinder(diameter: P.screwHole, height: P.screwDepth + 1).translated(z: -1)
                            .transformed(Pointing.frame(at: p, y: end.y(P.screwY)))
                    }
                }
                engraving("point \(Revision.label(Revision.pointShell))", size: 4.0)
                    .scaled(x: -1)
                    .rotated(x: 90°)
                    .translated(x: Frame.pocketX0 + P.pocketWidth / 2, y: Frame.pocketY0 + 0.4, z: -0.6)
            }
            .adding {
                Ledges()
                Pointing.headerJoint.added
                Pointing.socketJoint.added.transformed(Pointing.v)
            }
            .subtracting { Pointing.padPocket }
    }
}

/// The SW5 switch's opening and seat chamfer as in the key module, in the key module frame, under a keycap well through
/// the plateau: the pocket that the top housing sits in widens at 45° to `keycapWell`, so a pressed keycap clears the
/// plateau and the slope prints without support. The key sits 0.2 mm inside the socket face, so on V the well breaks
/// through it; `faceChamfer` then bevels the face down at 45° from the pocket's edge, where a ridge would print on nothing.
struct SW5Opening: Geometry3D {
    var faceChamfer = false
    var body: any Geometry3D {
        let at = Vector3D(Frame.bx(P.keyX), Frame.by(P.keyYs[0]), 0), c = P.seatChamfer
        let w = P.keycapWell, rise = (w - P.switchFlange) / 2
        if faceChamfer {
            let edge = Vector2D(at.x - P.switchFlange / 2, P.plateTopZ)
            alongColumn([edge, edge - Vector2D(10, 10), Vector2D(edge.x - 10, 20), Vector2D(edge.x, 20)], from: at.y - w / 2, length: w)
        }
        Box(x: P.switchCutout, y: P.switchCutout, z: 10).aligned(at: .centerXY).translated(at + [0, 0, P.ceilingZ - 1])
        Box(x: P.switchFlange, y: P.switchFlange, z: 0.01).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ])
            .adding { Box(x: w, y: w, z: 20).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ + rise]) }
            .convexHull()
        Box(x: P.switchCutout, y: P.switchCutout, z: 0.01).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ - c])
            .adding { Box(x: P.switchCutout + 2 * c + 1, y: P.switchCutout + 2 * c + 1, z: 0.01).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ + 0.5]) }
            .convexHull()
    }
}

/// Floor of the pointing module, screwed on from below at six corner screws. Under K it is the key module's floor: pillars
/// under the board's end margins, the headers' clamp pads and rear stops, and locating tabs where K's side walls stand.
/// Under V it carries the loose sockets' cradles (`PointingCradles`) and ribs that press the pad against the skin
/// (`PointingPadRibs`). It goes on along K's board normal, like the key module's.
struct PointingFloor: Geometry3D {
    var body: any Geometry3D {
        let c = P.tabClearance, t = P.tabThickness, h = P.tabHeight
        alongColumn(Pointing.floorSection)
            .adding {
                for x in P.pillarXs {
                    for y in P.pillarYs {
                        let z0 = KeyModuleFloor.rimAt(x) - 0.5
                        Cylinder(diameter: P.pillarDiameter, height: -z0).translated(x: x, y: Frame.by(y), z: z0)
                    }
                }
                for (y0, y1) in Pointing.wallTabYs {
                    for x in [Frame.pocketX0 + c, Frame.pocketX1 - c - t] {
                        Box(x: t, y: y1 - y0, z: h + 0.5).translated(x: x, y: y0, z: KeyModuleFloor.rimAt(x + t / 2) - 0.5)
                    }
                }
                for (x0, x1) in P.endTabXs {
                    for y in [Frame.pocketY0 + c, Frame.pocketY1 - c - t] {
                        let tab = Box(x: x1 - x0, y: t, z: h + 0.5).translated(x: x0, y: y, z: KeyModuleFloor.rimAt((x0 + x1) / 2) - 0.5)
                        tab
                        tab.transformed(Pointing.v)
                    }
                }
                ClampPadsAndRearStops(sockets: false)
                PointingCradles()
                PointingPadRibs()
            }
            .subtracting {
                for frame in Pointing.screwFrames { Counterbore().transformed(frame) }
                engraving("point floor \(Revision.label(Revision.pointFloor))", size: 4.0).rotated(z: 90°).translated(z: -0.6)
                    .transformed(Joint.levelFrame(x: 8, y: 48, level: Joint.rim))
            }
    }
}

/// Cradles on the floor for V's loose sockets: each body's clamp pad and rear stop (`ClampPadsAndRearStops`), and cheeks
/// beside its long sides, `tabClearance` off. Each body is glued into its cradle; the floor brings it up into its mouth
/// slot in V's socket wall, which locates and holds it at the mouth. The cheeks stop short of the SW5 switch's legs.
struct PointingCradles: Geometry3D {
    var body: any Geometry3D {
        let g = P.tabClearance, t = P.tabThickness, x0 = Frame.pocketX0 + 0.5   // the floor goes on 8° off V's board normal
        Union {
            ClampPadsAndRearStops(headers: false)
            for r in P.rowYs {
                for side in [-1.0, 1.0] {
                    let z0 = KeyModuleFloor.rimAt(x0 + P.cheekLength) - 0.5
                    Box(x: P.cheekLength, y: t, z: -0.5 - z0).aligned(at: .centerY)
                        .translated(x: x0, y: Frame.by(r) + side * (P.socketWidth / 2 + g + t / 2), z: z0)
                }
            }
        }
        .transformed(Pointing.v)
    }
}

/// Ribs across V's half of the pad, from the floor up to the pad's back with zero interference, so the pad stays pressed
/// against the skin if its adhesive lets go. Pressing half the pad's area holds the whole board; the other half lies over
/// K's board. They keep clear of the cradles, and of a channel along the seam for the wires.
struct PointingPadRibs: Geometry3D {
    var body: any Geometry3D {
        let t = P.tabThickness
        let ends = [P.padRibXs.0, P.padRibXs.1].map { Pointing.fromV(Vector2D($0, 0)) }
        let to = { (level: Double) in ends.map { $0 + Pointing.up * (level - Pointing.level($0)) } }
        let low = to(Pointing.bottom + 0.5), high = to(Pointing.padBack)
        for y in P.padRibYs {
            alongColumn([low[0], low[1], high[1], high[0]], from: y - t / 2, length: t)
        }
    }
}

/// The pointing module's parts as blocks, placed, and the room that the case keeps free for them.
enum PointingParts {
    /// K's board: a key module board with only SW5's switch and no sockets.
    static var board: any Geometry3D { BoardMockup(sockets: false, switchYs: [P.keyYs[0]]) }
    static var sockets: any Geometry3D {
        Union { for r in P.rowYs { SocketBody(rowY: r) } }.transformed(Pointing.v)
    }
    static var socketTails: any Geometry3D { ConnectorTails(headers: false).transformed(Pointing.v) }
    static var pad: any Geometry3D {
        Box(x: P.padWidth, y: P.padLength, z: P.padThickness).translated(x: -P.padWidth / 2, y: Pointing.padY0, z: -P.padThickness)
            .transformed(Pointing.frame(level: Pointing.padTop))
    }
    /// V's SW5 switch in V's plate, as in `BoardMockup`, its legs down from the housing with no board under them.
    static var vSwitch: any Geometry3D {
        let at = Vector3D(Frame.bx(P.keyX), Frame.by(P.keyYs[0]), 0)
        return Union {
            for (dx, d) in [(0.0, 3.3), (-5.5, 1.8), (5.5, 1.8)] {
                Cylinder(diameter: d, height: P.boardTopZ + 1.4).translated(at + [dx, 0, -1.4])
            }
            Box(x: P.switchCutout - 0.1, y: P.switchCutout - 0.1, z: P.plateToBoard).aligned(at: .centerXY).translated(at + [0, 0, P.boardTopZ])
            Box(x: 15.0, y: 15.0, z: 3.0).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ])
        }
        .transformed(Pointing.v)
    }
    /// Room under V's switch for its pins and their wires: where K's board has the hotswap socket, down to 4 mm under the
    /// plate.
    static var vSwitchRoom: any Geometry3D {
        let x = Frame.bx(P.keyX), y = Frame.by(P.keyYs[0])
        return Box(x: 14.5, y: 7.2, z: P.boardTopZ - (P.ceilingZ - 4)).translated(x: x - 7, y: y + 1.0, z: P.ceilingZ - 4)
            .transformed(Pointing.v)
    }
    /// Room over K's board front under the pad, for its 0805s.
    static var frontRoom: any Geometry3D {
        Box(x: P.boardWidth, y: P.padLength + 2 * P.padClearance, z: P.padOverParts)
            .translated(x: Frame.bx(P.boardOriginX), y: Pointing.padY0 - P.padClearance, z: P.boardTopZ)
    }
    static var shanks: [any Geometry3D] {
        let points = Pointing.screwPoints
        return Pointing.screwFrames.enumerated().map { i, frame in
            let length = Pointing.level(points[i % points.count]) - Pointing.bottom + P.screwDepth - 0.5
            return Cylinder(diameter: 2.0, height: length).translated(z: -0.5).transformed(frame) as any Geometry3D
        }
    }
}
