import Foundation
import Cadova

/// The populated key module board as simple blocks, for checking clearances: board, hotswap sockets, U1,
/// switch bodies, socket bodies, and the header body and pins as soldered in the 8° jig, and the connectors' tails
/// (`ConnectorTails`). Without `keyParts`, the socket board: the board and its sockets only.
struct BoardMockup: Geometry3D {
    var switches = true
    var keyParts = true
    var body: any Geometry3D {
        Box(x: P.boardWidth, y: P.boardLength, z: P.boardThickness)
            .translated(x: Frame.bx(P.boardOriginX), y: Frame.by(P.boardOriginY + P.boardLength), z: 0)
            .colored(.green)
        for y in keyParts ? P.keyYs : [] {   // hotswap socket above each key on the back; switch body on the front
            Box(x: 9.0, y: 6.7, z: 1.8).translated(x: Frame.bx(P.keyX - 2.0), y: Frame.by(y - 1.5), z: -1.8).colored(.black)
            if switches {
                // legs through the board (footprint: post at the key, pegs at ±5.5), 1.4 below its back, generous
                for (dx, d) in [(0.0, 3.3), (-5.5, 1.8), (5.5, 1.8)] {
                    Cylinder(diameter: d, height: 1.4).translated(x: Frame.bx(P.keyX + dx), y: Frame.by(y), z: -1.4).colored(.white)
                }
                Box(x: P.switchCutout - 0.1, y: P.switchCutout - 0.1, z: P.plateToBoard).aligned(at: .centerXY)
                    .translated(x: Frame.bx(P.keyX), y: Frame.by(y), z: P.boardTopZ).colored(.white, alpha: 0.5)
                Box(x: 15.0, y: 15.0, z: 3.0).aligned(at: .centerXY)
                    .translated(x: Frame.bx(P.keyX), y: Frame.by(y), z: P.plateTopZ).colored(.white, alpha: 0.5)
            }
        }
        if keyParts {
            Box(x: 10.4, y: 7.4, z: 1.75).aligned(at: .centerXY).translated(x: Frame.bx(81.325), y: Frame.by(96.7), z: -1.75).colored(.black)
        }
        for r in P.rowYs {
            SocketBody(rowY: r).colored(.gray)
            if keyParts { TiltedHeader(rowY: r).colored(.orange) }
        }
        ConnectorTails(headers: keyParts, margins: false).colored(.yellow)
    }
}

/// The connectors' tails on the board's back, as envelopes: each tail from the body's rear face to its tip, from the
/// far side of the pin axis up to the board. The datasheets draw the jog from the axis to the board but don't
/// dimension where it starts, so the envelope covers all of it.
///
/// With `margins`, the room that the case keeps free around them: each envelope grown by `P.tailClearance`, and each
/// foot (the straight end of the tail that lies on the pad) grown by `P.footClearance` for its solder fillet.
/// `headers` adds the headers' tails, tilted with their bodies.
struct ConnectorTails: Geometry3D {
    var headers = true
    var margins = true
    var body: any Geometry3D {
        let t = margins ? P.tailClearance : 0, f = P.footClearance, w = P.pinSquare
        let socketLow = P.pinAxisBelowBoard + P.socketTailThickness / 2, headerLow = P.pinAxisBelowBoard + P.pinSquare / 2
        for r in P.rowYs {
            for k in -1...1 {
                let y = Frame.by(r) + Double(k) * P.pinPitch
                Box(x: P.socketTail + 2 * t, y: w + 2 * t, z: socketLow + 2 * t)
                    .translated(x: P.socketDepth - t, y: y - w / 2 - t, z: -socketLow - t)
                if margins {
                    Box(x: P.socketFoot + 2 * f, y: w + 2 * f, z: P.socketTailThickness + 2 * f)
                        .translated(x: P.socketDepth + P.socketTail - P.socketFoot - f, y: y - w / 2 - f, z: -P.socketTailThickness - f)
                }
                if headers {
                    // untilted, x from the pivot (the foot's tip), as in `TiltedHeader`
                    Union {
                        Box(x: P.headerTailReach + 2 * t, y: w + 2 * t, z: headerLow + 2 * t).translated(x: -t, y: y - w / 2 - t, z: -headerLow - t)
                        if margins {
                            Box(x: P.headerFoot + 2 * f, y: w + 2 * f, z: w + 2 * f).translated(x: -f, y: y - w / 2 - f, z: -w - f)
                        }
                    }
                    .transformed(Joint.headerTilt)
                }
            }
        }
    }
}

/// Room that the case keeps free beside each connector body's long sides for the glue that stakes it to the board:
/// `P.stakingRoom` wide, from the body's top face up to the board, along the part of the body that lies over the
/// board (the header's, tilted with it).
struct StakingRoom: Geometry3D {
    var headers = true
    var body: any Geometry3D {
        let g = P.stakingRoom, top = P.bodyFloat + P.bodyHeight
        let onBoard = P.socketDepth - Frame.bx(P.boardOriginX)
        for r in P.rowYs {
            for side in [-1.0, 1.0] {
                Box(x: onBoard, y: g, z: top).aligned(at: .centerY)
                    .translated(x: Frame.bx(P.boardOriginX), y: Frame.by(r) + side * (P.socketWidth + g) / 2, z: -top)
                if headers {
                    // up to the board at the body's outer end, which the tilt puts farthest from it
                    let reach = tan(P.jointAngle.radians) * (P.headerTailReach + P.headerBodyDepth)
                    Box(x: P.headerBodyDepth, y: g, z: top + reach).aligned(at: .centerY)
                        .translated(x: P.headerTailReach, y: Frame.by(r) + side * (P.headerWidth + g) / 2, z: -top)
                        .transformed(Joint.headerTilt)
                        .intersecting { Box(x: 100, y: 200, z: 20).translated(x: -50, y: -50, z: -20) }
                }
            }
        }
    }
}

/// A socket body at connector row `rowY` (board frame), mouth at x = 0.
struct SocketBody: Geometry3D {
    let rowY: Double
    var body: any Geometry3D {
        Box(x: P.socketDepth, y: P.socketWidth, z: P.bodyHeight).aligned(at: .centerY)
            .translated(x: 0, y: Frame.by(rowY), z: -P.bodyFloat - P.bodyHeight)
    }
}

/// A header as soldered in the 8° jig at connector row `rowY` (board frame): body and pins, and optionally the
/// feet that lie on the board's back behind the body. Built untilted with x from the pivot, then tilted about it.
struct TiltedHeader: Geometry3D {
    let rowY: Double
    var feet = false
    var body: any Geometry3D {
        let body0 = P.headerTailReach
        Box(x: P.headerBodyDepth, y: P.headerWidth, z: P.bodyHeight).aligned(at: .centerY)
            .translated(x: body0, y: Frame.by(rowY), z: -P.bodyFloat - P.bodyHeight)
            .adding {
                for k in -1...1 {
                    Box(x: P.pinLength + 0.3, y: P.pinSquare, z: P.pinSquare).aligned(at: .centerY, .centerZ)
                        .translated(x: body0 + P.headerBodyDepth - 0.3, y: Frame.by(rowY) + Double(k) * P.pinPitch, z: P.pinAxisZ)
                    if feet {
                        Box(x: body0 + 0.3, y: P.pinSquare, z: P.pinSquare).aligned(at: .centerY)
                            .translated(y: Frame.by(rowY) + Double(k) * P.pinPitch, z: -P.pinSquare)
                    }
                }
            }
            .transformed(Joint.headerTilt)
    }
}
