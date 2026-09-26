import Cadova

/// The populated key module board as simple blocks, for checking clearances: board, hotswap sockets, U1,
/// switch bodies, socket bodies, and the header body and pins as soldered in the 8° jig.
struct BoardMockup: Geometry3D {
    var switches = true
    var body: any Geometry3D {
        Box(x: P.boardWidth, y: P.boardLength, z: P.boardThickness)
            .translated(x: Frame.bx(P.boardOriginX), y: Frame.by(P.boardOriginY), z: 0)
            .colored(.green)
        for y in P.keyYs {   // hotswap socket above each key on the back; switch body on the front
            Box(x: 9.0, y: 6.7, z: 1.8).translated(x: Frame.bx(P.keyX - 2.0), y: Frame.by(y - 8.2), z: -1.8).colored(.black)
            if switches {
                Box(x: P.switchCutout - 0.1, y: P.switchCutout - 0.1, z: P.plateToBoard).aligned(at: .centerXY)
                    .translated(x: Frame.bx(P.keyX), y: Frame.by(y), z: P.boardTopZ).colored(.white, alpha: 0.5)
                Box(x: 15.0, y: 15.0, z: 3.0).aligned(at: .centerXY)
                    .translated(x: Frame.bx(P.keyX), y: Frame.by(y), z: P.plateTopZ).colored(.white, alpha: 0.5)
            }
        }
        Box(x: 10.4, y: 7.4, z: 1.75).aligned(at: .centerXY).translated(x: Frame.bx(81.325), y: Frame.by(96.7), z: -1.75).colored(.black)
        for r in P.rowYs {
            Box(x: P.socketDepth, y: P.socketWidth, z: P.bodyHeight).aligned(at: .centerY)
                .translated(x: 0, y: Frame.by(r), z: -P.bodyFloat - P.bodyHeight).colored(.gray)
            // header, built untilted with x from the pivot, then tilted about it
            let body0 = P.headerTailReach
            Box(x: P.headerBodyDepth, y: P.headerWidth, z: P.bodyHeight).aligned(at: .centerY)
                .translated(x: body0, y: Frame.by(r), z: -P.bodyFloat - P.bodyHeight)
                .adding {
                    for k in -1...1 {
                        Box(x: P.pinLength + 0.3, y: P.pinSquare, z: P.pinSquare).aligned(at: .centerY, .centerZ)
                            .translated(x: body0 + P.headerBodyDepth - 0.3, y: Frame.by(r) + Double(k) * P.pinPitch, z: P.pinAxisZ)
                    }
                }
                .transformed(Transform3D([[Joint.c, 0, Joint.s, Joint.pivot.x], [0, 1, 0, 0], [-Joint.s, 0, Joint.c, 0], [0, 0, 0, 1]]))
                .colored(.orange)
        }
    }
}
