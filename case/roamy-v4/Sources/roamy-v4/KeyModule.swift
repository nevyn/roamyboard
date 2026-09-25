import Foundation
import Cadova

/// Top shell of a key module: plate with the switch openings, both side walls with the connector
/// slots, ledges that stop the board from above, screw posts and the joint hooks. Open at the
/// bottom; `KeyModuleFloor` closes it.
struct KeyModuleShell: Geometry3D {
    var body: any Geometry3D {
        OuterPrism()
            .intersecting { Box(x: 100, y: 200, z: P.height - P.floorThickness).translated(x: -20, y: -20, z: P.floorThickness) }
            .subtracting {
                // cavity under the plate
                Box(x: P.pocketWidth, y: P.pocketLength, z: P.height - P.plateThickness - P.floorThickness + 1)
                    .translated(x: Frame.pocketX0, y: Frame.pocketY0, z: P.floorThickness - 1)
                // switch openings
                for y in P.keyYs {
                    Box(x: P.switchCutout, y: P.switchCutout, z: P.plateThickness + 2)
                        .aligned(at: .centerXY)
                        .translated(x: Frame.bx(P.keyX), y: Frame.by(y), z: P.height - P.plateThickness - 1)
                }
                // socket mouths through the socket wall, open toward the floor
                for r in P.rowYs {
                    Box(x: P.wall + 1.5, y: P.socketWidth + 2 * P.boardClearance, z: P.boardBottomZ + 0.1)
                        .aligned(at: .centerY)
                        .translated(x: -0.5, y: Frame.by(r), z: 0)
                }
                // header pins through the header wall, open toward the floor
                for r in P.rowYs {
                    let width = 2 * P.pinPitch + P.pinSquare + 1.2
                    Box(x: P.wall + 3, y: width, z: P.pinAxisZ + P.pinSquare / 2 + 0.4)
                        .aligned(at: .centerY)
                        .translated(x: Frame.pocketX1 - 0.5, y: Frame.by(r), z: 0)
                }
                // windows for the previous module's hooks
                for y in JointHooks.hookYs {
                    Box(x: P.wall + 1.5, y: P.hookThickness + 2 * P.hookClearance, z: P.hookZ + P.hookHeight + 0.6)
                        .aligned(at: .centerY)
                        .translated(x: -0.5, y: y, z: 0)
                }
            }
            .adding {
                Ledges()
                ScrewPosts()
                JointHooks()
            }
    }
}

/// Stops the board from moving up: full-width ledges at both ends, a continuous ledge along the
/// header side, and tabs between the switch housings on the socket side, where the housings sit
/// 0.05 mm from the board edge.
struct Ledges: Geometry3D {
    var body: any Geometry3D {
        let z0 = P.boardTopZ
        let h = P.height - P.plateThickness - z0
        Box(x: P.pocketWidth, y: P.ledgeLength, z: h).translated(x: Frame.pocketX0, y: Frame.pocketY0, z: z0)
        Box(x: P.pocketWidth, y: P.ledgeLength, z: h).translated(x: Frame.pocketX0, y: Frame.pocketY1 - P.ledgeLength, z: z0)
        Box(x: P.ledgeWidth, y: P.pocketLength, z: h).translated(x: Frame.pocketX1 - P.ledgeWidth, y: Frame.pocketY0, z: z0)
        for y in P.socketSideTabYs {
            Box(x: P.ledgeWidth, y: 4.0, z: h).aligned(at: .centerY).translated(x: Frame.pocketX0, y: Frame.by(y), z: z0)
        }
    }
}

/// Posts under the board's bare end margins, grown out of the end walls; M2 screws come up
/// through the floor into them, and their tops push the board against the ledges.
struct ScrewPosts: Geometry3D {
    static var positions: [(Double, Double, Double)] {   // x, y, direction toward the end wall
        let xs = P.postFractions.map { Frame.pocketX0 + P.pocketWidth * $0 }
        return xs.map { ($0, Frame.pocketY0 + P.postInset, -1.0) } + xs.map { ($0, Frame.pocketY1 - P.postInset, 1.0) }
    }
    var body: any Geometry3D {
        let h = P.boardBottomZ - P.floorThickness
        for (x, y, toward) in Self.positions {
            Cylinder(diameter: P.postDiameter, height: h)
                .adding {
                    Box(x: P.postDiameter, y: P.postInset + 0.5, z: h)
                        .aligned(at: .centerX)
                        .translated(y: toward > 0 ? 0 : -(P.postInset + 0.5))
                }
                .subtracting { Cylinder(diameter: P.screwHole, height: P.screwDepth).translated(z: -0.01) }
                .translated(x: x, y: y, z: P.floorThickness)
        }
    }
}

/// Cantilever hooks on the header side, built in the neighbour's frame so they lie level in the
/// neighbour once the modules meet at the joint angle. The barb faces the module end and catches
/// on the inside of the neighbour's socket wall; pressing the hooks inward releases them.
struct JointHooks: Geometry3D {
    static var hookYs: [Double] { [Frame.by(P.boardOriginY + P.hookInset), Frame.by(P.boardOriginY + P.boardLength - P.hookInset)] }
    var body: any Geometry3D {
        for (i, y) in Self.hookYs.enumerated() {
            let outward = i == 0 ? -1.0 : 1.0
            let catchX = P.wall + P.boardClearance + P.hookClearance
            Box(x: P.hookReach + 1.0, y: P.hookThickness, z: P.hookHeight)
                .aligned(at: .centerY)
                .translated(x: -1.0)
                .adding {
                    Polygon([[catchX, 0], [catchX, P.hookBarb], [catchX + P.hookBarbRamp, 0]])
                        .extruded(height: P.hookHeight)
                        .scaled(y: outward)
                        .translated(y: outward * P.hookThickness / 2)
                }
                .translated(y: y, z: P.hookZ)
                .transformed(Frame.neighbour)
        }
    }
}

/// Flat floor screwed on from below. Its outline follows the shell at floor height.
struct KeyModuleFloor: Geometry3D {
    var body: any Geometry3D {
        OuterPrism()
            .intersecting { Box(x: 100, y: 200, z: P.floorThickness).translated(x: -20, y: -20) }
            .subtracting {
                for (x, y, _) in ScrewPosts.positions {
                    Cylinder(diameter: P.screwClearance, height: P.floorThickness + 1).translated(x: x, y: y, z: -0.5)
                    Cylinder(diameter: P.screwHeadDiameter, height: P.screwHeadDepth + 0.5).translated(x: x, y: y, z: -0.5)
                }
            }
    }
}
