import Foundation
import Cadova

/// Top shell of a key module: plate with the switch openings and pockets, walls with bottom-open connector
/// slots, ledges that stop the board from above, and the joint: guide pins on the header side, holes with
/// spring flaps on the socket side. Open at the bottom, so the board drops in from below;
/// `KeyModuleFloor` closes it.
struct KeyModuleShell: Geometry3D {
    /// Print pose only: a one-layer membrane over each switch opening at the pocket floor, so the pocket rim
    /// prints on it instead of over air (cut it out after printing), and a break-away fin under each guide pin.
    var printAids = false
    var body: any Geometry3D {
        alongColumn(keystone(from: Joint.rim, to: Joint.top))
            .subtracting {
                // cavity under the plate
                Box(x: P.pocketWidth, y: P.pocketLength, z: P.ceilingZ + 20)
                    .translated(x: Frame.pocketX0, y: Frame.pocketY0, z: -20)
                for y in P.keyYs {
                    let at = Vector3D(Frame.bx(P.keyX), Frame.by(y), 0)
                    let cutTop = printAids ? P.plateTopZ - P.layer : P.plateTopZ + 10
                    Box(x: P.switchCutout, y: P.switchCutout, z: cutTop - P.ceilingZ + 1).aligned(at: .centerXY).translated(at + [0, 0, P.ceilingZ - 1])
                    Box(x: P.switchFlange, y: P.switchFlange, z: 10).aligned(at: .centerXY).translated(at + [0, 0, P.plateTopZ])
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
                for end in ColumnEnd.allCases {
                    GuideHole(end: end)
                    ScrewHoles(end: end)
                }
                // revision on the SW1 end wall's inner face, below the end ledge, read from the pocket
                engraving("shell \(Revision.label(Revision.shell))", size: 4.0)
                    .scaled(x: -1)
                    .rotated(x: 90°)
                    .translated(x: Frame.pocketX0 + P.pocketWidth / 2, y: Frame.pocketY0 + 0.4, z: -0.6)
            }
            .adding {
                Ledges()
                for end in ColumnEnd.allCases {
                    GuidePin(end: end).transformed(Joint.neighbourTransform)
                    if printAids { GuidePinFin(end: end) }
                }
            }
    }
}

enum ColumnEnd: CaseIterable {
    case sw1, sw5
    /// y at depth d into the end wall from its outer face.
    func y(_ d: Double) -> Double { self == .sw1 ? d : P.outerLength - d }
    /// A y-z section given at the SW1 end, placed at this end.
    func section(_ points: [Vector2D]) -> [Vector2D] { points.map { Vector2D(y($0.x), $0.y) } }
    /// An x-y outline given at the SW1 end, placed at this end.
    func plan(_ points: [Vector2D]) -> [Vector2D] { points.map { Vector2D($0.x, y($0.y)) } }
}

/// Guide pin section: flat sides, 45° gables above and below, so it prints without support in any
/// orientation and centres in its hole both ways.
func guideSection(grow g: Double = 0, scale k: Double = 1) -> [Vector2D] {
    let w = (P.guideWidth * k) / 2 + g, h = (P.guideFlat * k) / 2 + g, gable = (P.guideWidth * k) / 2 + g
    let y = P.guideY, z = P.guideZ
    return [[y - w, z - h], [y, z - h - gable], [y + w, z - h], [y + w, z + h], [y, z + h + gable], [y - w, z + h]]
}

/// Tooth on the flap's inner face, in plan (x, depth into the end wall): 45° lead-in toward the
/// neighbour, catch face at `toothCatchAngle`.
func toothPlan(grow g: Double = 0) -> [Vector2D] {
    let x0 = P.toothX - P.toothBase / 2 - g, x1 = P.toothX + P.toothBase / 2 + g
    let d0 = P.flapThickness - 0.01, h = P.guideClearance + P.toothEngagement + g
    let catchRun = h / tan(P.toothCatchAngle.radians)
    return [[x0, d0], [x1, d0], [x1 - catchRun, d0 + h], [x0 + h, d0 + h]]
}

/// The neighbour's guide pin, in the neighbour's frame (x from its socket face): it roots in this module's
/// end wall and reaches into the neighbour's hole, notched for the flap's tooth.
struct GuidePin: Geometry3D {
    let end: ColumnEnd
    var body: any Geometry3D {
        let tip = P.guideReach, taperStart = tip - P.guideTaper
        acrossColumn(end.section(guideSection()), from: -P.guideRoot, to: taperStart)
            .adding {
                acrossColumn(end.section(guideSection()), from: taperStart - 0.01, to: taperStart)
                    .adding { acrossColumn(end.section(guideSection(scale: P.guideTipScale)), from: tip - 0.01, to: tip) }
                    .convexHull()
            }
            .subtracting {
                // notch: the tooth grown by the clearance, run 0.3 further toward the root so the faces close first
                Polygon(end.plan(toothPlan(grow: P.guideClearance))).extruded(height: 20).translated(z: -10)
                Polygon(end.plan(toothPlan(grow: P.guideClearance))).extruded(height: 20).translated(x: -0.3, z: -10)
            }
    }
}

/// Print aid: a break-away fin under each guide pin, from its top ridge to this module's top plane, which lies on
/// the bed in the print pose. The pin prints nearly flat and would otherwise droop toward its tip.
struct GuidePinFin: Geometry3D {
    let end: ColumnEnd
    var body: any Geometry3D {
        let apex = P.guideZ + P.guideFlat / 2 + P.guideWidth / 2
        let tipApex = P.guideZ + (P.guideFlat / 2 + P.guideWidth / 2) * P.guideTipScale
        let ridge = [Vector2D(-0.2, apex), Vector2D(P.guideReach - P.guideTaper, apex),
                     Vector2D(P.guideReach - 0.2, tipApex + (apex - tipApex) * 0.2 / P.guideTaper)].map(Joint.neighbour)
        let top = [ridge[2].x, ridge[0].x].map { Vector2D($0, Joint.z(level: Joint.top, x: $0)) }
        alongColumn(ridge + top, from: min(end.y(P.guideY - P.finThickness / 2), end.y(P.guideY + P.finThickness / 2)),
                    length: P.finThickness)
    }
}

/// Hole for the previous module's guide pin, and the spring flap over it: the end wall's outer skin, cut free
/// behind and at its free edge, hinged on the socket-face side. Top and bottom edges are the module's own
/// top and rim, so the flap prints standing on the bed and bends within its layers.
struct GuideHole: Geometry3D {
    let end: ColumnEnd
    var body: any Geometry3D {
        let slit = [P.flapThickness, P.flapThickness + P.flapSlit]
        let holeApex = P.guideZ + P.guideFlat / 2 + P.guideWidth / 2 + P.guideClearance
        acrossColumn(end.section(guideSection(grow: P.guideClearance)), from: -1, to: P.guideReach + 0.3)
            .adding {
                // relief along the top ridge for the guide pin fin's break-off scar
                acrossColumn(end.section([[P.guideY - 0.4, holeApex - 0.4], [P.guideY + 0.4, holeApex - 0.4], [P.guideY, holeApex + 0.3]]),
                             from: -1, to: P.guideReach + 0.3)
                // slit behind the flap, through top and rim
                Polygon(end.plan([[P.flapHinge, slit[0]], [P.flapFreeEdge + P.flapSlot, slit[0]],
                                  [P.flapFreeEdge + P.flapSlot, slit[1]], [P.flapHinge, slit[1]]]))
                    .extruded(height: 40).translated(z: -20)
                // free edge
                Polygon(end.plan([[P.flapFreeEdge, -1], [P.flapFreeEdge + P.flapSlot, -1],
                                  [P.flapFreeEdge + P.flapSlot, slit[1]], [P.flapFreeEdge, slit[1]]]))
                    .extruded(height: 40).translated(z: -20)
            }
            .subtracting {
                // the tooth stays
                let h = P.guideFlat + P.guideWidth + 2 * P.guideClearance   // the hole's full height
                Polygon(end.plan(toothPlan())).extruded(height: h).translated(z: P.guideZ - h / 2)
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
    var body: any Geometry3D {
        let z0 = P.boardTopZ, h = P.ceilingZ - P.boardTopZ + 0.01
        Box(x: P.pocketWidth, y: P.ledgeLength, z: h).translated(x: Frame.pocketX0, y: Frame.pocketY0, z: z0)
        Box(x: P.pocketWidth, y: P.ledgeLength, z: h).translated(x: Frame.pocketX0, y: Frame.pocketY1 - P.ledgeLength, z: z0)
        Box(x: P.ledgeWidth, y: P.pocketLength, z: h).translated(x: Frame.pocketX1 - P.ledgeWidth, y: Frame.pocketY0, z: z0)
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
                // counterbores bridged in two layers (a slot, then a square), so the floor prints bottom-down without
                // the head's ceiling drooping into the bore
                for end in ColumnEnd.allCases {
                    for x in P.screwXs {
                        let d = P.screwHeadDepth, l = P.layer, s = P.screwClearance
                        Cylinder(diameter: P.screwHeadDiameter, height: d + 0.5).translated(z: -0.5)
                            .adding {
                                Box(x: s, y: P.screwHeadDiameter, z: l).aligned(at: .centerXY).translated(z: d)
                                Box(x: s, y: s, z: 2 * l).aligned(at: .centerXY).translated(z: d)
                                Cylinder(diameter: s, height: P.floorThickness + 2).translated(z: d + 2 * l)
                            }
                            .transformed(Self.frame(x: x, y: end.y(P.screwY)))
                    }
                }
            }
    }
}

/// Print placements: the shell with its top on the bed, the floor with its bottom on the bed.
enum PrintPose {
    static var shell: Transform3D {
        let u = Joint.up, t = Joint.along, o = Joint.centre
        // x' = (p - o)·t, y' = -y, z' = top - (p - o)·up
        return Transform3D([[t.x, 0, t.y, -(o.x * t.x + o.y * t.y)],
                            [0, -1, 0, P.outerLength],
                            [-u.x, 0, -u.y, Joint.top + (o.x * u.x + o.y * u.y)],
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
