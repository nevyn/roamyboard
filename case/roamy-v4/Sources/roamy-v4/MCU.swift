import Foundation
import Cadova

/// Layout of the MCU module, in the key module frame: it is the last key module's neighbour and takes the joint's
/// socket side. Its board (the socket board, or later the MCU board) lies where a key module's board does, with the
/// nice!view over it under a window in the plate. The bay beside it holds the battery at one column end and the
/// nice!nano at `P.usbEnd`, with the battery jack, power switch and reset button between them. The top lies higher
/// than the key modules' so the view fits over the board; the bottom and rim are theirs.
///
/// Positions along the column are distances from the battery end's outer face (`d`); `place` puts a part built with
/// +y toward the USB end at such a distance, on a level.
enum MCU {
    static let joint = SocketSideJoint()

    // Levels
    static var viewX0: Double { Frame.pocketX0 + P.ledgeWidth + 0.1 }
    /// The view's back: `viewAir` over the board where they come closest, under the view's outer edge.
    static var viewBack: Double { Joint.level(Vector2D(viewX0 + P.viewWidth + 0.3, P.boardTopZ + P.viewAir)) }
    static var top: Double { max(Joint.top, viewBack + P.viewBackParts + P.viewPCB + P.viewGlass + P.viewLip) }
    static var ceiling: Double { top - P.mcuPlate }

    // Across the column: x where lines perpendicular to the top cross the board's back
    static var bayX0: Double { Frame.pocketX1 + 0.5 }
    static var freeWallX: Double { bayX0 + P.batteryWidth + 2 * P.batterySlack }   // the free wall's inner face
    static var freeFaceX: Double { freeWallX + P.wall }
    /// The key module's end walls, with the joint's reserved blocks and the corner screws, reach this far.
    static var stripX1: Double { P.screwXs.max()! + 3.5 }
    static var outerScrewX: Double { freeWallX - P.outerScrewInset.x }

    // Along the column, from the battery end
    static var batteryD: Double { 7.2 + P.batterySlack }
    static var jackD: Double { batteryD + P.batteryLength + P.batterySlack + 6.0 }   // mating face; room for the plug
    static let switchD = 62.0
    static let resetD = 73.0
    static var nanoD: Double { P.outerLength - P.bayEndWall - 0.1 - P.nanoLength }
    static var viewD: Double { (P.outerLength - P.viewLength) / 2 }
    static var jackX: Double { bayX0 + 3.0 }
    static var nanoX: Double { (bayX0 + freeWallX - P.nanoWidth) / 2 }
    static var switchX: Double { freeWallX + P.freeWallPocket.switch - P.switchBody.u }
    static var resetX: Double { freeWallX + P.freeWallPocket.reset - P.resetBody.u }

    /// A part built with local x across the column from `x`, +y toward the USB end from `d`, z up from `level`.
    static func place(_ part: any Geometry3D, x: Double, d: Double, level: Double) -> any Geometry3D {
        P.usbEnd == .sw5
            ? part.transformed(Joint.levelFrame(x: x, y: d, level: level))
            : part.scaled(y: -1).transformed(Joint.levelFrame(x: x, y: P.outerLength - d, level: level))
    }
    /// y of a point at distance d from the battery end.
    static func y(_ d: Double) -> Double { P.usbEnd == .sw5 ? d : P.outerLength - d }

    /// Section between two levels, from the socket face to the free face.
    static func section(from lower: Double, to upper: Double) -> [Vector2D] {
        [Joint.corner(Joint.socketFace, lower), Joint.onPerpendicular(x: freeFaceX, level: lower),
         Joint.onPerpendicular(x: freeFaceX, level: upper), Joint.corner(Joint.socketFace, upper)]
    }
}

/// Top of the MCU module: plate with the view window, walls, the socket side of the joint, the socket board's ledges,
/// pockets in the free wall for the power switch and reset button, the USB opening, and lugs. Open at the bottom;
/// `MCUFloor` closes it.
struct MCUShell: Geometry3D {
    var body: any Geometry3D {
        alongColumn(MCU.section(from: Joint.rim, to: MCU.top))
            .subtracting {
                MCUCavity()
                // socket mouths through the socket wall, open toward the floor, as in the key module
                for r in P.rowYs {
                    Box(x: P.wall + 2, y: P.socketWidth + 2 * P.boardClearance, z: 20 - P.bodyFloat + 0.15)
                        .aligned(at: .centerY)
                        .translated(x: -1, y: Frame.by(r), z: -20)
                }
                MCU.joint.removed
                for end in ColumnEnd.allCases { ScrewHoles(end: end) }
                MCUParts.viewPocket
                MCUParts.usbOpening
                // revision on the plate's underside over the bay, read from below
                MCU.place(engraving("mcu \(Revision.label(Revision.mcuShell))", size: 4.0).scaled(x: -1).rotated(z: 90°)
                    .translated(z: -0.6), x: MCU.bayX0 + 8, d: 30, level: MCU.ceiling)
            }
            .adding {
                Ledges(ceilingZ: Joint.z(level: MCU.ceiling + 0.3, x: Frame.pocketX0), headerSide: P.boardSideTabYs)
                MCU.joint.added
                MCUStops()
                MCUChambers()
                // holds the nice!nano down on the top of its USB-C port
                let portTop = Joint.rim + P.nanoRise + P.portHeight + 0.1
                MCU.place(Box(x: 6, y: 4, z: MCU.ceiling - portTop + 0.5),
                          x: MCU.nanoX + (P.nanoWidth - 6) / 2, d: MCU.nanoD + P.nanoLength - 4.5, level: portTop)
                for end in ColumnEnd.allCases {
                    Cylinder(diameter: P.bossDiameter, height: MCU.ceiling - Joint.rim + 0.5)
                        .transformed(Joint.levelFrame(x: MCU.outerScrewX, y: end.y(P.outerScrewInset.y), level: Joint.rim))
                    Lug(faceX: MCU.freeFaceX, outward: 1, end: end, droop: P.jointAngle, top: MCU.top, rootFrom: Joint.rim)
                }
            }
            .subtracting {
                for end in ColumnEnd.allCases {
                    Cylinder(diameter: P.screwHole, height: P.screwDepth + 1).translated(z: -1)
                        .transformed(Joint.levelFrame(x: MCU.outerScrewX, y: end.y(P.outerScrewInset.y), level: Joint.rim))
                }
                MCUParts.freeWallPockets
            }
    }
}

/// The shell's inside, from below the rim to the ceiling: the key module's pocket beside the socket wall, between its
/// end walls, and the bay out to the free wall, between thin end walls.
struct MCUCavity: Geometry3D {
    var body: any Geometry3D {
        let x0 = Frame.pocketX0, low = Joint.rim - 5
        alongColumn([Vector2D(x0, Joint.z(level: low, x: x0)), Joint.onPerpendicular(x: MCU.freeWallX, level: low),
                     Joint.onPerpendicular(x: MCU.freeWallX, level: MCU.ceiling), Vector2D(x0, Joint.z(level: MCU.ceiling, x: x0))])
            .intersecting {
                Union {
                    Box(x: MCU.stripX1 + 5, y: P.pocketLength, z: 60).translated(x: -5, y: Frame.pocketY0, z: -30)
                    Box(x: 100, y: P.outerLength - 2 * P.bayEndWall, z: 60).translated(x: MCU.stripX1, y: P.bayEndWall, z: -30)
                }
            }
    }
}

/// Stops that hang from the plate: guides at the nice!nano's sides that reach 0.6 mm down its board edge, a stop behind
/// it and a prop over it; stops at the battery's ends, leaving its lead free to leave any side (the socket board's edge
/// stops the battery toward the board); walls at the view's short ends; legs beside the socket board's bay-side edge.
struct MCUStops: Geometry3D {
    var body: any Geometry3D {
        let t = P.tabThickness, w = P.nanoWidth, l = P.nanoLength, nano = Joint.rim + P.nanoRise
        let hang = { (level: Double) in MCU.ceiling - level + 0.5 }
        for x in [MCU.nanoX - 0.15 - t, MCU.nanoX + w + 0.15] {
            for d in [MCU.nanoD + 2, MCU.nanoD + l - 12] {   // the front pair clear of the screw boss
                MCU.place(Box(x: t, y: 4, z: hang(nano + 0.4)), x: x, d: d, level: nano + 0.4)
            }
        }
        MCU.place(Box(x: w / 2, y: 1.0, z: hang(nano - 0.5)), x: MCU.nanoX + w / 4, d: MCU.nanoD - 1.1, level: nano - 0.5)
        // prop over the nano's middle, so it lies level in the upturned shell until the floor's rib clamps it
        let prop = nano + P.nanoHeight + 0.1
        MCU.place(Box(x: w / 2, y: l / 2, z: hang(prop)), x: MCU.nanoX + w / 4, d: MCU.nanoD + l / 4, level: prop)
        // the view's short ends: a wall at its flex end, corner posts at its header end so its wires can leave
        let v = P.viewWidth, e = 1.2, span = v + 0.3 + 2 * e, wall = hang(MCU.viewBack)
        MCUParts.viewPlaced(Union {
            Box(x: span, y: e, z: wall).translated(x: -0.15 - e, y: -0.35 - e)
            for x in [-0.15 - e, v + 0.15 - 2] {
                Box(x: 2 + e, y: e, z: wall).translated(x: x, y: P.viewLength + 0.15)
            }
        })
        // legs beside the socket board's bay-side edge, under the board-side tabs: they stop it sliding into the bay
        for (y0, y1) in P.boardSideTabYs {
            let z0 = -0.5, z1 = Joint.z(level: MCU.ceiling + 0.3, x: Frame.pocketX1)
            Box(x: 0.9, y: y1 - y0, z: z1 - z0).translated(x: Frame.pocketX1, y: y0, z: z0)
        }
        let battery = Joint.rim + 2.0, b = P.batterySlack
        for d in [MCU.batteryD - b - t, MCU.batteryD + P.batteryLength + b] {
            for x in [MCU.bayX0 + 4, MCU.bayX0 + P.batteryWidth - 14] {   // clear of the outer screw bosses
                MCU.place(Box(x: 6, y: t, z: hang(battery)), x: x, d: d, level: battery)
            }
        }
    }
}

/// Chambers for the power switch and the reset button, hanging from the plate down to the rim behind their wall pockets:
/// a wall behind each body takes the press, and walls beyond its terminals stop it along the column. The corners stay
/// open for the wires, and the switch's back wall clears its signal terminals.
struct MCUChambers: Geometry3D {
    var body: any Geometry3D {
        let t = P.chamberWall, h = MCU.ceiling - Joint.rim + 0.5, gap = 0.1
        let b = P.switchBody, r = P.resetBody
        let switchSides = P.switchSpan / 2 + 0.2, resetSides = P.resetSpan / 2 + 0.3
        MCU.place(Union {
            Box(x: t, y: 3, z: h - 0.5).translated(x: -gap - t, y: b.y / 2 - 1.5, z: 0.5)
            for y in [b.y / 2 - switchSides - t, b.y / 2 + switchSides] {
                Box(x: b.u - P.freeWallPocket.switch + gap + t + 0.5, y: t, z: h).translated(x: -gap - t, y: y)
            }
        }, x: MCU.switchX, d: MCU.switchD - b.y / 2, level: Joint.rim)
        MCU.place(Union {
            Box(x: t, y: r.y - 0.6, z: h).translated(x: -gap - t, y: 0.3)
            for y in [r.y / 2 - resetSides - t, r.y / 2 + resetSides] {
                Box(x: r.u - P.freeWallPocket.reset + gap + t + 0.5, y: t, z: h).translated(x: -gap - t, y: y)
            }
        }, x: MCU.resetX, d: MCU.resetD - r.y / 2, level: Joint.rim)
    }
}

/// Floor of the MCU module, screwed on like the key module's: pillars under the board's end margins, locating tabs, the
/// socket board's clamp pads and rear stops (`ClampPadsAndRearStops`), a rib that presses the nice!nano against the shell's
/// post on its port, a tongue that closes the port's slot, a seat for the battery jack, and reliefs for the switch's and
/// button's pegs. Everything that locates the parts sideways hangs from the shell, so they go into the
/// upturned shell before the floor closes it.
struct MCUFloor: Geometry3D {
    var body: any Geometry3D {
        let c = P.tabClearance, t = P.tabThickness, h = P.tabHeight, rim = Joint.rim
        alongColumn(MCU.section(from: Joint.bottom, to: rim))
            .adding {
                for x in P.pillarXs {
                    for y in P.pillarYs {
                        let z0 = KeyModuleFloor.rimAt(x) - 0.5
                        Cylinder(diameter: P.pillarDiameter, height: -z0).translated(x: x, y: Frame.by(y), z: z0)
                    }
                }
                // locating tabs: along the socket wall and the strip's end walls as in the key module, along the free
                // wall beside the nano, and along the bay's end wall at the battery
                for (y0, y1) in P.sideTabYs {
                    let x = Frame.pocketX0 + c
                    Box(x: t, y: Frame.by(y0) - Frame.by(y1), z: h + 0.5).translated(x: x, y: Frame.by(y1), z: KeyModuleFloor.rimAt(x + t / 2) - 0.5)
                }
                for (x0, x1) in P.endTabXs {
                    for y in [Frame.pocketY0 + c, Frame.pocketY1 - c - t] {
                        Box(x: x1 - x0, y: t, z: h + 0.5).translated(x: x0, y: y, z: KeyModuleFloor.rimAt((x0 + x1) / 2) - 0.5)
                    }
                }
                MCU.place(Box(x: t, y: 24, z: h + 0.5), x: MCU.freeWallX - c - t, d: 80, level: rim - 0.5)
                MCU.place(Box(x: 20, y: t, z: h + 0.5), x: MCU.stripX1 + 2, d: P.bayEndWall + c, level: rim - 0.5)
                ClampPadsAndRearStops(headers: false)
                // nice!nano: rib under its middle; the shell holds it at the sides, behind and above
                MCU.place(Box(x: P.nanoWidth / 2, y: P.nanoLength / 2, z: P.nanoRise + 0.01),
                          x: MCU.nanoX + P.nanoWidth / 4, d: MCU.nanoD + P.nanoLength / 4, level: rim - 0.01)
                MCUParts.usbTongue
                // battery jack: seat and cheeks
                let j = P.jackBody
                MCU.place(Box(x: j.u + 2.7, y: j.y, z: P.jackSeat + 0.01), x: MCU.jackX - 1.35, d: MCU.jackD, level: rim - 0.01)
                for x in [MCU.jackX - 1.35, MCU.jackX + j.u + 0.15] {
                    MCU.place(Box(x: 1.2, y: j.y, z: P.jackSeat + 3), x: x, d: MCU.jackD, level: rim - 0.01)
                }
            }
            .subtracting {
                for end in ColumnEnd.allCases {
                    for x in P.screwXs { Counterbore().transformed(KeyModuleFloor.frame(x: x, y: end.y(P.screwY))) }
                    Counterbore().transformed(Joint.levelFrame(x: MCU.outerScrewX, y: end.y(P.outerScrewInset.y), level: Joint.bottom))
                }
                // jack tails, through; switch pegs and button bosses
                MCU.place(Box(x: 3.4, y: 1.4, z: 20), x: MCU.jackX + P.jackBody.u / 2 - 1.7, d: MCU.jackD + P.jackPins - 0.7, level: rim - 10)
                MCU.place(Box(x: P.switchBody.u, y: P.switchBody.y, z: 1.0), x: MCU.switchX, d: MCU.switchD - P.switchBody.y / 2, level: rim - 0.7)
                MCU.place(Box(x: P.resetBody.u, y: P.resetBody.y, z: 1.0), x: MCU.resetX, d: MCU.resetD - P.resetBody.y / 2, level: rim - 0.7)
                // revision on the inside face
                MCU.place(engraving("mcu floor \(Revision.label(Revision.mcuFloor))", size: 4.0).rotated(z: 90°).translated(z: -0.6),
                          x: MCU.nanoX + P.nanoWidth + 4, d: 66, level: rim)
            }
    }
}

/// The MCU module's parts as blocks, placed, and the cuts that follow them.
enum MCUParts {
    static var socketBoard: any Geometry3D { BoardMockup(switches: false, keyParts: false) }
    static var battery: any Geometry3D {
        MCU.place(Box(x: P.batteryWidth, y: P.batteryLength, z: P.batteryThickness), x: MCU.bayX0 + P.batterySlack, d: MCU.batteryD, level: Joint.rim)
    }
    static func nanoPlaced(_ part: any Geometry3D) -> any Geometry3D {
        MCU.place(part, x: MCU.nanoX, d: MCU.nanoD, level: Joint.rim + P.nanoRise)
    }
    static var nano: any Geometry3D {
        nanoPlaced(Box(x: P.nanoWidth, y: P.nanoLength, z: P.nanoHeight)
            .adding {
                Box(x: P.portWidth, y: P.portDepth, z: P.portHeight)
                    .translated(x: (P.nanoWidth - P.portWidth) / 2, y: P.nanoLength + P.portOverhang - P.portDepth)
            })
    }
    /// View, built with its header edge toward the USB end: back parts, PCB, glass.
    static func viewPlaced(_ part: any Geometry3D) -> any Geometry3D {
        MCU.place(part, x: MCU.viewX0, d: MCU.viewD, level: MCU.viewBack)
    }
    static var view: any Geometry3D {
        viewPlaced(Box(x: P.viewWidth, y: P.viewLength, z: P.viewBackParts + P.viewPCB)
            .adding { Box(x: P.viewWidth - 1, y: P.viewLength - 4, z: P.viewGlass).translated(x: 0.5, y: 1, z: P.viewBackParts + P.viewPCB) })
    }
    static var switchPart: any Geometry3D {
        let b = P.switchBody, s = P.switchSlider
        return MCU.place(Box(x: b.u, y: b.y, z: b.h)
            .adding {
                Box(x: s.u, y: s.y, z: s.h).translated(x: b.u, y: (b.y - s.y) / 2, z: (b.h - s.h) / 2)
                // terminals, as envelopes at the mounting face: ground at the ends, signal behind
                Box(x: b.u, y: P.switchSpan, z: 0.3).translated(y: (b.y - P.switchSpan) / 2)
                Box(x: P.switchTails, y: 4.5, z: 0.3).translated(x: -P.switchTails, y: (b.y - 4.5) / 2)
            },
            x: MCU.switchX, d: MCU.switchD - b.y / 2, level: Joint.rim)
    }
    static var resetPart: any Geometry3D {
        let b = P.resetBody, a = P.resetActuator
        return MCU.place(Box(x: b.u, y: b.y, z: b.h)
            .adding {
                Box(x: a.u, y: a.y, z: a.h).translated(x: b.u, y: (b.y - a.y) / 2, z: b.h - a.h)
                Box(x: b.u, y: P.resetSpan, z: 0.3).translated(y: (b.y - P.resetSpan) / 2)   // terminals at both ends
            },
            x: MCU.resetX, d: MCU.resetD - b.y / 2, level: Joint.rim)
    }
    static var jack: any Geometry3D {
        let j = P.jackBody
        return MCU.place(Box(x: j.u, y: j.y, z: j.h), x: MCU.jackX, d: MCU.jackD, level: Joint.rim + P.jackSeat)
    }
    /// Everything but the board, for checks.
    static var all: any Geometry3D { Union { battery; nano; view; switchPart; resetPart; jack } }

    /// Pocket that the view drops into from below, and the window over its active area.
    static var viewPocket: any Geometry3D {
        let m = P.windowMargin, a = P.viewActiveInset, glassTop = P.viewBackParts + P.viewPCB + P.viewGlass
        return viewPlaced(Box(x: P.viewWidth + 0.3, y: P.viewLength + 0.5, z: glassTop + 0.3).translated(x: -0.15, y: -0.35, z: -0.3)
            .adding {
                Box(x: P.viewWidth - 2 * a.side + 2 * m, y: P.viewLength - a.header - a.far + 2 * m, z: 10)
                    .translated(x: a.side - m, y: a.far - m, z: glassTop - 0.1)
            })
    }
    /// USB-C opening through the end wall, open toward the floor, and a recess around it for the plug's overmould.
    static var usbOpening: any Geometry3D {
        let face = P.nanoLength + P.portOverhang, cx = P.nanoWidth / 2, cz = P.portHeight / 2
        return nanoPlaced(
            Rectangle(x: P.portWidth + 0.6, y: P.portHeight + 0.6).aligned(at: .center).rounded(radius: 1.0)
                .extruded(height: 10).rotated(x: -90°).translated(x: cx, y: P.nanoLength - 0.5, z: cz)
                .adding {
                    // open down to the rim, so the nano drops in port first; the floor closes the slot
                    Box(x: P.portWidth + 0.6, y: 10, z: cz + P.nanoRise + 1).translated(x: cx - (P.portWidth + 0.6) / 2, y: P.nanoLength - 0.5, z: -P.nanoRise - 1)
                    Rectangle(x: 13.4, y: 7.6).aligned(at: .center).rounded(radius: 1.5)
                        .extruded(height: 5).rotated(x: -90°).translated(x: cx, y: face, z: cz)
                })
    }
    /// The floor's tongue that closes the USB-C slot to 0.1 under the port, out to the port's face, short of the
    /// overmould's recess.
    static var usbTongue: any Geometry3D {
        let slot = P.portWidth + 0.6 - 0.2, face = P.nanoLength + P.portOverhang
        return nanoPlaced(Box(x: slot, y: face - P.nanoLength + 0.8, z: P.nanoRise - 0.1 + 0.01)
            .translated(x: (P.nanoWidth - slot) / 2, y: P.nanoLength - 0.8, z: -P.nanoRise - 0.01))
    }
    /// Pockets in the free wall's inner face for the switch and the button, their terminals included, open toward the
    /// floor, and their openings.
    static var freeWallPockets: any Geometry3D {
        let b = P.switchBody, s = P.switchSlider, r = P.resetBody, a = P.resetActuator
        return Union {
            MCU.place(Box(x: b.u + 0.3, y: P.switchSpan + 0.3, z: b.h + 0.2 + 0.5).translated(x: -0.15, y: (b.y - P.switchSpan) / 2 - 0.15, z: -0.5)
                .adding { Box(x: 5, y: s.y + s.travel + 0.4, z: s.h + 0.4).translated(x: b.u - 0.1, y: (b.y - s.y - s.travel) / 2 - 0.2, z: (b.h - s.h) / 2 - 0.2) },
                x: MCU.switchX, d: MCU.switchD - b.y / 2, level: Joint.rim)
            MCU.place(Box(x: r.u + 0.3, y: P.resetSpan + 0.3, z: r.h + 0.2 + 0.5).translated(x: -0.15, y: (r.y - P.resetSpan) / 2 - 0.15, z: -0.5)
                .adding {   // down through the rim: the floor closes it, where a one-layer skin would print as loose strands
                    Box(x: 5, y: a.y + 0.3, z: r.h + 0.15 + 0.5).translated(x: r.u - 0.1, y: (r.y - a.y) / 2 - 0.15, z: -0.5)
                },
                x: MCU.resetX, d: MCU.resetD - r.y / 2, level: Joint.rim)
        }
    }
}

/// Check fixtures: an M2 shank in each floor screw, and the USB-C plug's overmould as it sits when plugged in.
enum MCUFixtures {
    static var shanks: [any Geometry3D] {
        let length = Joint.rim - Joint.bottom + P.screwDepth - 0.5
        return ColumnEnd.allCases.flatMap { end in
            P.screwXs.map { x in Cylinder(diameter: 2.0, height: length).translated(z: -0.5).transformed(KeyModuleFloor.frame(x: x, y: end.y(P.screwY))) as any Geometry3D }
                + [Cylinder(diameter: 2.0, height: length).translated(z: -0.5)
                    .transformed(Joint.levelFrame(x: MCU.outerScrewX, y: end.y(P.outerScrewInset.y), level: Joint.bottom))]
        }
    }
    /// Room for wires across the socket board's bay-side edge, between the board-side tabs: over the board past the
    /// view's header end, and at each connector row from under the board (where the pads are) round its edge and over
    /// the battery into the bay.
    static var wirePaths: any Geometry3D {
        let x0 = Frame.pocketX1 - 4, edge = Frame.pocketX1 + 0.9, x1 = MCU.bayX0 + 3, over = P.boardTopZ + 0.4
        return Union {
            Box(x: x1 - x0, y: 2, z: 1.2).translated(x: x0, y: MCU.y(MCU.viewD + P.viewLength + 3) - 1, z: over)
            for r in P.rowYs {
                Box(x: edge - x0, y: 2, z: 1.2).translated(x: x0, y: Frame.by(r) - 1, z: -1.9)
                Box(x: edge - Frame.pocketX1 - 0.1, y: 2, z: over + 1.2 + 1.9).translated(x: Frame.pocketX1 + 0.1, y: Frame.by(r) - 1, z: -1.9)
                Box(x: x1 - Frame.pocketX1 + 1, y: 2, z: 1.2).translated(x: Frame.pocketX1 - 1, y: Frame.by(r) - 1, z: over)
            }
        }
    }
    static var plug: any Geometry3D {
        let face = P.nanoLength + P.portOverhang
        return MCUParts.nanoPlaced(Box(x: 12.2, y: 20, z: 6.5).aligned(at: .centerX, .centerZ)
            .translated(x: P.nanoWidth / 2, y: face + 0.3, z: P.portHeight / 2))
    }
}
