import Foundation
import Cadova

/// Joint hardware on one side of a module, in the module frame (`Frame`, `Joint`). The host keeps its body solid
/// inside `reserved` and builds none of its own features there, subtracts `removed` with its own cuts, and adds
/// `added` after them. `KeyModuleShell` hosts both sides; a module with one joint face hosts only that side.
/// Everything that couples modules (guide pins, latch, release) lives behind this protocol, so hosts never
/// depend on how the latch works.
///
/// ```swift
/// outline
///     .subtracting { hostCuts; joint.removed }
///     .adding { hostParts; joint.added; if printAids { joint.printAids } }
/// ```
protocol JointSide: Sendable {
    /// Joint parts, unioned after the host's cuts. They may protrude past the joint face (guide pins).
    @GeometryBuilder3D var added: any Geometry3D { get }
    /// Cuts inside `reserved`, subtracted from the host's body.
    @GeometryBuilder3D var removed: any Geometry3D { get }
    /// The part of the host's body that the joint owns: one block per column end, from the joint face and the end
    /// face inward, short of the corner screws. Only its intersection with the host's outline counts.
    @GeometryBuilder3D var reserved: any Geometry3D { get }
    /// Print-pose-only parts (break-away fins), for a host printed with its top on the bed (`PrintPose.shell`).
    @GeometryBuilder3D var printAids: any Geometry3D { get }
    /// Space outside the body that the host must leave empty: the slab past the joint face, which the neighbour
    /// sweeps during slide-on, and finger room at the end faces where the user releases the latch. `added`
    /// may occupy it.
    @GeometryBuilder3D var keepOut: any Geometry3D { get }
}

/// Depth of `reserved` from the end face: up to the corner screws' holes, less 0.1.
private var reservedDepth: Double { P.screwY - P.screwHole / 2 - 0.1 }
private let keepOutReach = 15.0
private var levelSpan: (Double, Double) { (Joint.bottom - 2, Joint.top + 2) }

/// The slab past a joint face, along the whole module, bounded by the face's own plane.
private func beyondFace(_ face: Vector2D, outward: Vector2D) -> any Geometry3D {
    let a = Joint.corner(face, levelSpan.0), b = Joint.corner(face, levelSpan.1)
    let ring = [a, a + outward * keepOutReach, b + outward * keepOutReach, b]
    let area = zip(ring, ring.dropFirst() + [ring[0]]).map { $0.x * $1.y - $1.x * $0.y }.reduce(0, +)
    return alongColumn(area > 0 ? ring : ring.reversed(), from: -1, length: P.outerLength + 2)
}

/// Guide pins at both column ends of the header face, rooted in this module's end walls.
struct HeaderSideJoint: JointSide {
    var added: any Geometry3D {
        for end in ColumnEnd.allCases { GuidePin(end: end).transformed(Joint.neighbourTransform) }
    }
    var removed: any Geometry3D { Empty() }
    var reserved: any Geometry3D {
        // in the neighbour's frame: from this module's header face (the neighbour's x = 0) in past the pin roots
        let depth = P.guideRoot + 0.5
        for end in ColumnEnd.allCases {
            Box(x: depth, y: reservedDepth, z: 40)
                .translated(x: -depth, y: end == .sw1 ? 0 : P.outerLength - reservedDepth, z: -20)
                .transformed(Joint.neighbourTransform)
        }
    }
    var printAids: any Geometry3D {
        for end in ColumnEnd.allCases { GuidePinFin(end: end) }
    }
    var keepOut: any Geometry3D {
        let f = Joint.headerFace
        beyondFace(f, outward: Vector2D(f.y, -f.x))
    }
}

/// Guide holes with their latch flaps at both column ends of the socket face.
struct SocketSideJoint: JointSide {
    /// x extent of `reserved` and of the finger room: past the guide hole's end and the flap slot.
    private var reach: Double { P.guideReach + 1.0 }
    var added: any Geometry3D { Empty() }
    var removed: any Geometry3D {
        for end in ColumnEnd.allCases { GuideHole(end: end) }
    }
    var reserved: any Geometry3D {
        for end in ColumnEnd.allCases {
            Box(x: reach + 1, y: reservedDepth, z: 40)
                .translated(x: -1, y: end == .sw1 ? 0 : P.outerLength - reservedDepth, z: -20)
        }
    }
    var printAids: any Geometry3D { Empty() }
    var keepOut: any Geometry3D {
        let f = Joint.socketFace
        beyondFace(f, outward: Vector2D(-f.y, f.x))
        // finger room at the end faces, over the flaps
        let z0 = Joint.z(level: Joint.bottom, x: 0), z1 = Joint.z(level: Joint.top, x: 0)
        for end in ColumnEnd.allCases {
            Box(x: reach + 1, y: 10, z: z1 - z0)
                .translated(x: -1, y: end == .sw1 ? -10 : P.outerLength, z: z0)
        }
    }
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

