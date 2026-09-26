import Foundation
import Cadova

/// Terminator module: the header-side part of a key module's outline, printed solid in one piece, with the joint's
/// header side and two headers at the key module's positions. The headers go in at an embed pause, joined by a
/// wire that ties the +3.3V pin of the first connector row to the DATA pin of the second, so the neighbour's
/// 74HC165 shifts in ones after its own byte and the MCU reads the end-of-chain sentinel. Lugs on the free face.
///
/// It shares the key module frame: its neighbour sits at `Joint.neighbourTransform`, and it prints top-down in
/// `PrintPose.shell`.
struct TerminatorModule: Geometry3D {
    static let joint = HeaderSideJoint()
    /// Rows that carry a header; the wire joins pin `wiredPins[0]` of the first to pin `wiredPins[1]` of the second.
    static let rows = [P.rowYs[0], P.rowYs[1]]
    /// Board y of the wired pins: +3.3V (J_RIGHT1 pin 2) and DATA (J_RIGHT2 pin 3), from KeyModule.kicad_pcb.
    static let wiredPins = [P.rowYs[0], P.rowYs[1] + P.pinPitch]

    /// A point on the free face, which stands perpendicular to the top behind the feet pockets and the wire groove.
    static var freeFace: Vector2D { Vector2D(Joint.pivot.x - P.footClearance - EmbedPockets.groove - P.terminatorWall, 0) }

    static var outline: any Geometry3D {
        let onFree = { (level: Double) in Joint.onPerpendicular(x: freeFace.x, level: level) }
        return alongColumn([onFree(Joint.bottom), Joint.corner(Joint.headerFace, Joint.bottom),
                            Joint.corner(Joint.headerFace, Joint.top), onFree(Joint.top)])
    }

    /// Level of the embed pause: the pockets end here, and the first layer after the pause bridges them. On a
    /// layer boundary in the print pose, above every embedded part.
    static var pauseLevel: Double {
        let body0 = P.headerTailReach, tip = body0 + P.headerBodyDepth - 0.3 + P.pinLength + 0.3
        let lowest = [Vector2D(body0 + P.headerBodyDepth, -P.bodyHeight), Vector2D(tip, P.pinAxisZ - P.pinSquare / 2),
                      Vector2D(-P.footClearance, -P.pinSquare - P.wireDiameter), Vector2D(body0, -P.pinSquare - P.wireDiameter)]
            .map { Joint.level(Joint.tilt($0 + Joint.pivot) ) }.min()!
        let height = ((Joint.top - lowest + P.embedClearance) / P.layer).rounded(.up) * P.layer
        return Joint.top - height
    }
    /// Print height (`PrintPose.shell`, top on the bed) at which to pause and drop the headers in.
    static var pauseHeight: Double { Joint.top - pauseLevel }

    var body: any Geometry3D {
        Self.outline
            .subtracting {
                EmbedPockets()
                // the neighbour's socket mouths reach just past the seam
                for r in P.rowYs {
                    Box(x: 2, y: P.socketWidth + 2 * P.boardClearance, z: P.bodyHeight + 2 * P.boardClearance)
                        .aligned(at: .centerY)
                        .translated(x: -1, y: Frame.by(r), z: -P.bodyFloat - P.bodyHeight - P.boardClearance)
                        .transformed(Joint.neighbourTransform)
                }
                Self.joint.removed
                // revision on the bottom, read from below
                let x = Self.freeFace.x + 4
                engraving("term \(Revision.label(Revision.terminator))", size: 4.0)
                    .scaled(x: -1)
                    .rotated(z: 90°)
                    .translated(z: -0.4)
                    .transformed(KeyModuleFloor.frame(x: x, y: 30))
            }
            .adding {
                Self.joint.added
                for end in ColumnEnd.allCases { Lug(faceX: Self.freeFace.x, outward: -1, end: end) }
            }
    }
}

/// Pockets for the embedded headers and their feet, each swept toward the bottom up to the pause level so that
/// they lie open to the nozzle at the pause, and a groove for the wire between the rows, open through the bottom:
/// covering it would take a bridge longer than the pockets'. The wire leaves its feet toward the free face.
struct EmbedPockets: Geometry3D {
    static var groove: Double { P.wireDiameter + 0.2 }
    var body: any Geometry3D {
        let c = P.embedClearance, f = P.footClearance, body0 = P.headerTailReach
        let ys = TerminatorModule.rows.map(Frame.by)
        let span = P.headerWidth / 2 + c
        var parts: [any Geometry3D] = []
        for y in ys {
            parts.append(Box(x: P.headerBodyDepth + 2 * c, y: P.headerWidth + 2 * c, z: P.bodyHeight + 2 * c)
                .translated(x: body0 - c, y: y - span, z: -P.bodyHeight - c))
            parts.append(Box(x: P.pinLength + 3, y: 2 * P.pinPitch + P.pinSquare + 2 * c, z: P.pinSquare + 2 * c)
                .translated(x: body0 + P.headerBodyDepth - 0.5, y: y - P.pinPitch - P.pinSquare / 2 - c, z: P.pinAxisZ - P.pinSquare / 2 - c))
            // feet, with room for the solder and the wire's ends
            let feet = P.pinPitch + P.pinSquare / 2 + f
            parts.append(Box(x: body0 + f, y: 2 * feet, z: P.pinSquare + P.wireDiameter + f)
                .translated(x: -f, y: y - feet, z: -P.pinSquare - P.wireDiameter))
        }
        let down = Joint.up * -20, tilted = parts.map { $0.transformed(Joint.headerTilt) }
        return Union {
            for part in tilted {
                part.adding { part.translated(x: down.x, z: down.y) }.convexHull()
            }
        }
            .intersecting { alongColumn(levelBand(from: TerminatorModule.pauseLevel, to: Joint.top + 5)) }
            .adding {
                // behind the feet pockets, so that it opens into them only on their walls along the bridges
                let pins = TerminatorModule.wiredPins.map(Frame.by), w = EmbedPockets.groove
                let groove = Box(x: w + 0.01, y: abs(pins[1] - pins[0]), z: P.wireDiameter + 0.1)
                    .translated(x: -f - w, y: pins.min()!, z: -P.pinSquare - P.wireDiameter - 0.1)
                    .transformed(Joint.headerTilt)
                groove.adding { groove.translated(x: down.x, z: down.y) }.convexHull()
            }
    }
}

/// The parts that go in at the pause: both headers with their feet, and the wire between the wired pins' feet,
/// modelled along its run in the groove.
struct TerminatorEmbedded: Geometry3D {
    var body: any Geometry3D {
        for r in TerminatorModule.rows { TiltedHeader(rowY: r, feet: true) }
        let ys = TerminatorModule.wiredPins.map(Frame.by)
        Cylinder(diameter: P.wireDiameter, height: abs(ys[1] - ys[0]))
            .rotated(x: -90°)
            .translated(x: -P.footClearance - EmbedPockets.groove / 2, y: ys.min()!, z: -P.pinSquare - P.wireDiameter / 2)
            .transformed(Joint.headerTilt)
    }
}

/// Section between two levels over the whole module width and beyond.
func levelBand(from lower: Double, to upper: Double) -> [Vector2D] {
    let x0 = -30.0, x1 = 80.0
    return [Vector2D(x0, Joint.z(level: lower, x: x0)), Vector2D(x1, Joint.z(level: lower, x: x1)),
            Vector2D(x1, Joint.z(level: upper, x: x1)), Vector2D(x0, Joint.z(level: upper, x: x0))]
}
