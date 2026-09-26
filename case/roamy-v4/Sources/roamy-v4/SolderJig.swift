import Foundation
import Cadova

/// Soldering jig for the key module board, built in the module frame from the same numbers as the case, so
/// the connectors end up where the case expects them. The board lies front down in a pocket (front passives
/// sink into pockets), its back flush with the jig top. Past the edges:
/// - socket nests: floor in the board plane (the body rests on the board), snug sides, a stop against the mouth;
/// - header nests: a cradle under the body tilted by the joint angle about the inboard end of its pads, a
///   slot under each pin, and a stop at the body's outer edge.
/// Every cut is open toward the component side, so the soldered board lifts straight out.
struct SolderJig: Geometry3D {
    static let fit = 0.1              // connector bodies, per side
    static let pinFit = 0.08          // header pins, per side
    static let boardFit = 0.15
    static let base = 3.0             // under the board's front
    static let socketBlockTop = -2.0  // module z; nest walls reach 2.0 of the socket's 2.5
    static let headerBlockTop = -3.9  // below the tilted pin tips
    static let border = 4.0
    // Front passives, board frame: x, y, rotated 90°. Pocket 0805 plus fillets.
    static let passives: [(Double, Double, Bool)] = [(83.6, 60.0, false), (72.3, 81.0, true), (76.25, 99.4, false),
                                                     (79.6, 99.9, true), (82.95, 99.4, false), (76.0, 118.4, false)]
    static let passivePocket = (length: 4.4, width: 3.0, depth: 1.6)   // 0805 plus solder fillets

    static var x0: Double { -3.0 }
    static var x1: Double { Frame.headerEdgeX + P.headerBodyDepth + P.pinLength + 4.0 }
    static var y0: Double { Frame.by(P.boardOriginY) - border }
    static var y1: Double { Frame.by(P.boardOriginY + P.boardLength) + border }
    static var bottom: Double { P.boardThickness + base }

    /// A point of the untilted header (x from the pivot, z from the board's back), as soldered.
    static func header(_ x: Double, _ z: Double) -> Vector2D { Joint.tilt(Joint.pivot + Vector2D(x, z)) }
    static var bodyNear: Double { P.headerTailReach }
    static var bodyFar: Double { P.headerTailReach + P.headerBodyDepth }
    /// Outer edge of the header body on its board side: the outermost point of the body, where the stop touches.
    static var stopCorner: Vector2D { header(bodyFar, -P.bodyFloat) }

    var body: any Geometry3D {
        let edgeS = Frame.bx(P.boardOriginX), edgeH = Frame.headerEdgeX
        let length = Self.y1 - Self.y0
        Box(x: Self.x1 - Self.x0, y: length, z: Self.bottom).translated(x: Self.x0, y: Self.y0)
            .adding {
                Box(x: edgeS - Self.boardFit - Self.x0, y: length, z: -Self.socketBlockTop)
                    .translated(x: Self.x0, y: Self.y0, z: Self.socketBlockTop)
                Box(x: Self.x1 - edgeH - Self.boardFit, y: length, z: -Self.headerBlockTop)
                    .translated(x: edgeH + Self.boardFit, y: Self.y0, z: Self.headerBlockTop)
            }
            .subtracting {
                // board, and everything above its back
                Box(x: P.boardWidth + 2 * Self.boardFit, y: P.boardLength + 2 * Self.boardFit, z: 20 + P.boardThickness)
                    .translated(x: edgeS - Self.boardFit, y: Frame.by(P.boardOriginY) - Self.boardFit, z: -20)
                for (x, y, turned) in Self.passives {
                    let (l, w, d) = Self.passivePocket
                    Box(x: turned ? w : l, y: turned ? l : w, z: d + 0.01).aligned(at: .centerXY)
                        .translated(x: Frame.bx(x), y: Frame.by(y), z: P.boardThickness)
                }
                // finger holes under the board's ends, to push it out
                for y in [P.boardOriginY, P.boardOriginY + P.boardLength] {
                    Cylinder(radius: 5, height: Self.bottom + 1).translated(x: Frame.bx(P.keyX), y: Frame.by(y), z: -0.5)
                }
                for r in P.rowYs {
                    // socket: mouth against the stop at x = 0, body on the board plane
                    Box(x: edgeS + 1, y: P.socketWidth + 2 * Self.fit, z: 20 - P.bodyFloat)
                        .aligned(at: .centerY).translated(y: Frame.by(r), z: -20)
                    // header body cradle, up to the stop
                    let near = Self.header(Self.bodyNear, -P.bodyFloat), stop = Self.stopCorner
                    alongColumn([near, stop, [stop.x, -20], [near.x, -20]],
                                from: Frame.by(r) - P.headerWidth / 2 - Self.fit, length: P.headerWidth + 2 * Self.fit)
                    // pin slots, floors under the pins, past their tips
                    let pinTop = -P.pinAxisBelowBoard + P.pinSquare / 2
                    let a = Self.header(Self.bodyFar - 0.3, pinTop), b = Self.header(Self.bodyFar + P.pinLength + 2.0, pinTop)
                    for k in -1...1 {
                        alongColumn([a, b, [b.x, -20], [a.x, -20]],
                                    from: Frame.by(r) + Double(k) * P.pinPitch - P.pinSquare / 2 - Self.pinFit,
                                    length: P.pinSquare + 2 * Self.pinFit)
                    }
                }
                // orientation marks, readable from above once the jig is turned over
                for (label, y) in [("SW5", Frame.by(P.keyYs[0])), ("SW1", Frame.by(P.keyYs[4]))] {
                    engraving(label, size: 4.0)
                        .scaled(y: -1)
                        .translated(x: (edgeH + Self.boardFit + Self.x1) / 2, y: y, z: Self.headerBlockTop - 0.6)
                }
                // tilt and revision, along the column between the middle rows
                engraving("\(Int(P.jointAngle.degrees))° jig \(Revision.label(Revision.jig))")
                    .rotated(z: 90°)
                    .scaled(y: -1)
                    .translated(x: (edgeH + Self.boardFit + Self.x1) / 2, y: Frame.by((P.rowYs[1] + P.rowYs[2]) / 2), z: Self.headerBlockTop - 0.6)
            }
    }

    /// Jig turned over: base on the bed, board pocket up.
    static var printPose: Transform3D {
        Transform3D([[1, 0, 0, 0], [0, -1, 0, 0], [0, 0, -1, bottom], [0, 0, 0, 1]])
    }
}
