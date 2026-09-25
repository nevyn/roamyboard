import Cadova

/// The populated key module board as simple blocks, for checking clearances in the viewer:
/// board, hotswap sockets, U1, socket and header bodies with pins at the joint angle.
struct BoardMockup: Geometry3D {
    var body: any Geometry3D {
        let z0 = P.boardBottomZ
        Box(x: P.boardWidth, y: P.boardLength, z: P.boardThickness)
            .translated(x: Frame.bx(P.boardOriginX), y: Frame.by(P.boardOriginY), z: z0)
            .colored(.green)
        for y in P.keyYs {   // hotswap socket, above each key on the back
            Box(x: 9.0, y: 6.7, z: 1.8).translated(x: Frame.bx(P.keyX - 2.0), y: Frame.by(y - 8.2), z: z0 - 1.8).colored(.black)
            Box(x: P.switchCutout - 0.1, y: P.switchCutout - 0.1, z: 2.2 + 3.0).aligned(at: .centerXY)
                .translated(x: Frame.bx(P.keyX), y: Frame.by(y), z: P.boardTopZ).colored(.white, alpha: 0.4)
        }
        Box(x: 10.4, y: 7.4, z: 1.75).aligned(at: .centerXY).translated(x: Frame.bx(81.325), y: Frame.by(96.7), z: z0 - 1.75).colored(.black)
        for r in P.rowYs {
            Box(x: 8.5, y: P.socketWidth, z: P.socketHeight).aligned(at: .centerY)
                .translated(x: Frame.bx(P.boardOriginX - P.socketOverhang), y: Frame.by(r), z: z0 - P.socketHeight).colored(.gray)
            Box(x: 2.5, y: P.headerWidth, z: 2.5).aligned(at: .centerY)
                .translated(x: Frame.bx(P.boardOriginX + P.boardWidth - 2.5), y: Frame.by(r), z: z0 - 2.5).colored(.gray)
            for k in -1...1 {
                Box(x: P.pinLength + 2.5, y: P.pinSquare, z: P.pinSquare).aligned(at: .centerY, .centerZ)
                    .translated(x: -2.5, y: Frame.by(r) + Double(k) * P.pinPitch, z: P.pinAxisZ)
                    .transformed(Frame.neighbour)
                    .colored(.yellow)
            }
        }
    }
}
