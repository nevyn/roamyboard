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
                for i in 0..<2 {
                    JointLevers.channel(index: i)
                    JointLevers.window(index: i)
                    SpringPanels.cuts(index: i)
                }
            }
            .adding {
                Ledges()
                JointLevers()
            }
            .subtracting { CornerScrews.holes() }
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

/// Vertical M2 screw holes in the corner blocks of the end walls, from the floor up.
struct CornerScrews {
    static var positions: [(Double, Double)] {
        [P.cornerScrewY, P.outerLength - P.cornerScrewY].flatMap { y in P.cornerScrewXs.map { ($0, y) } }
    }
    static func holes() -> any Geometry3D {
        Union {
            for (x, y) in positions {
                Cylinder(diameter: P.screwHole, height: P.screwDepth).translated(x: x, y: y, z: P.floorThickness - 0.01)
            }
        }
    }
}

/// Spring panels: a strip of each end wall, hinged along a vertical line at the socket-wall
/// corner and free on its other edges, thinned from the inside. Pressing it swings the lever it
/// carries inward and frees the barb.
struct SpringPanels {
    static func outer(_ i: Int) -> Double { i == 0 ? 0 : P.outerLength }     // outer face y
    static func dir(_ i: Int) -> Double { i == 0 ? 1.0 : -1.0 }               // from the outer face into the module
    static func cuts(index i: Int) -> any Geometry3D {
        let y0 = outer(i), d = dir(i)
        let slotW = P.panelSlot
        return Union {
        // thin the panel from the inside
        Box(x: P.panelX1 - P.panelX0, y: P.endWall - P.panelThickness + 0.01, z: P.panelZ1 - P.panelZ0)
            .translated(x: P.panelX0, y: d > 0 ? P.panelThickness : y0 - P.endWall, z: P.panelZ0)
        // slots around the free end and the top and bottom edges, through the wall
        Box(x: slotW, y: P.endWall + 2, z: P.panelZ1 - P.panelZ0 + 2 * slotW).translated(x: P.panelX1, y: y0 - 1 - (d < 0 ? P.endWall : 0), z: P.panelZ0 - slotW)
        Box(x: P.panelX1 - P.panelX0 + slotW, y: P.endWall + 2, z: slotW).translated(x: P.panelX0, y: y0 - 1 - (d < 0 ? P.endWall : 0), z: P.panelZ0 - slotW)
        Box(x: P.panelX1 - P.panelX0 + slotW, y: P.endWall + 2, z: slotW).translated(x: P.panelX0, y: y0 - 1 - (d < 0 ? P.endWall : 0), z: P.panelZ1)
        }
    }
}

/// Rigid levers, one per module end, rooted in the spring panel and reaching past the header face
/// into the neighbour's socket wall. Level inside this module; the part past the header face is
/// built in the neighbour's frame so the barb's catch face lies flat on the neighbour's wall.
struct JointLevers: Geometry3D {
    static var leverYs: [Double] { [Frame.by(P.boardOriginY + P.leverInset), Frame.by(P.boardOriginY + P.boardLength - P.leverInset)] }
    static func outward(_ i: Int) -> Double { i == 0 ? -1.0 : 1.0 }
    static var faceX: Double { Frame.headerFaceX(z: P.leverZ + P.leverHeight / 2) }

    static func lever(index i: Int) -> any Geometry3D {
        let y = leverYs[i]
        let panelInner = i == 0 ? P.panelThickness - 0.4 : P.outerLength - P.panelThickness + 0.4
        // level inside this module, from the panel to the header face
        let inner = Box(x: faceX + 0.5 - P.leverRootX, y: P.leverThickness, z: P.leverHeight)
            .aligned(at: .centerY)
            .translated(x: P.leverRootX, y: y, z: P.leverZ)
            .adding {
                Box(x: 3.0, y: abs(y - panelInner) + P.leverThickness / 2, z: P.leverHeight)
                    .translated(x: P.leverRootX - 1.5, y: min(panelInner, y - P.leverThickness / 2), z: P.leverZ)
            }
        // the part past the face, with the barb, in the neighbour's frame so the catch face meets its wall flat
        let catchX = P.wall + P.boardClearance + P.leverClearance
        let catchRise = P.leverBarb / tan(P.leverCatchAngle.radians)
        let outer = Box(x: P.leverReach + 0.5, y: P.leverThickness, z: P.leverHeight)
            .aligned(at: .centerY)
            .translated(x: -0.5, y: y, z: P.leverZ)
            .adding {
                Polygon([[catchX - catchRise, 0], [catchX, P.leverBarb], [catchX + P.leverBarbRamp, 0]])
                    .extruded(height: P.leverHeight)
                    .scaled(y: outward(i))
                    .translated(y: y + outward(i) * P.leverThickness / 2, z: P.leverZ)
            }
            .transformed(Frame.neighbour)
        return inner.adding { outer }
    }

    /// Room for the lever to swing: a channel through the header wall, wider on the inward side.
    static func channel(index i: Int) -> any Geometry3D {
        let inward = -outward(i)
        return Box(x: faceX + 1.0 - P.leverRootX, y: P.leverThickness + 2 * P.leverClearance + P.leverTravel, z: P.leverHeight + 2 * P.leverClearance)
            .translated(x: P.leverRootX, y: leverYs[i] - P.leverThickness / 2 - P.leverClearance + (inward < 0 ? -P.leverTravel : 0), z: P.leverZ - P.leverClearance)
    }

    /// Window in this module's socket wall for the previous module's lever, open toward the floor.
    static func window(index i: Int) -> any Geometry3D {
        let inward = -outward(i)
        let w = P.leverThickness + 2 * P.leverClearance + P.leverTravel
        return Box(x: P.wall + 1.5, y: w, z: P.leverZ + P.leverHeight + 0.6)
            .translated(x: -0.5, y: leverYs[i] - P.leverThickness / 2 - P.leverClearance + (inward < 0 ? -P.leverTravel : 0), z: 0)
    }

    var body: any Geometry3D {
        for i in 0..<Self.leverYs.count { Self.lever(index: i) }
    }
}

/// Flat floor screwed on from below into the end-wall corners. Pillars under the board's bare
/// end margins push it up against the ledges; they arrive with the floor, after the board.
struct KeyModuleFloor: Geometry3D {
    var body: any Geometry3D {
        OuterPrism()
            .intersecting { Box(x: 100, y: 200, z: P.floorThickness).translated(x: -20, y: -20) }
            .adding {
                for y in [P.pillarY, P.outerLength - P.pillarY] {
                    for x in P.pillarXs {
                        Cylinder(diameter: P.pillarDiameter, height: P.boardBottomZ - 0.1 - P.floorThickness + 0.01)
                            .translated(x: x, y: y, z: P.floorThickness - 0.01)
                    }
                }
            }
            .subtracting {
                for (x, y) in CornerScrews.positions {
                    Cylinder(diameter: P.screwClearance, height: P.floorThickness + 1).translated(x: x, y: y, z: -0.5)
                    Cylinder(diameter: P.screwHeadDiameter, height: P.screwHeadDepth + 0.5).translated(x: x, y: y, z: -0.5)
                }
            }
    }
}

/// Coupon: a plate with Choc openings from 13.7 to 14.2 mm, to find the size that clicks on this printer.
struct ChocCutoutCoupon: Geometry3D {
    var body: any Geometry3D {
        let sizes = [13.7, 13.8, 13.9, 14.0, 14.1, 14.2]
        let pitch = 19.05
        let margin = 5.0                      // room for the switch flange (15 mm) and a label below each hole
        Box(x: pitch * Double(sizes.count) + 4, y: pitch + 2 * margin, z: P.plateThickness)
            .subtracting {
                for (i, s) in sizes.enumerated() {
                    let cx = 2 + pitch * (Double(i) + 0.5)
                    Box(x: s, y: s, z: P.plateThickness + 2).aligned(at: .centerXY)
                        .translated(x: cx, y: margin + pitch / 2, z: -1)
                    Text(String(format: "%.1f", s)).withFontSize(3.0)
                        .extruded(height: 0.5).aligned(at: .centerXY)
                        .translated(x: cx, y: 1.8, z: P.plateThickness - 0.4)
                }
            }
    }
}
