import Foundation
import Cadova

/// Numbers the case builds on. Board and connector figures come from electronics/Electronics.md and the
/// hanxia drawings in electronics/datasheets; switch figures from the Kailh Choc v1 plate-mount convention.
/// Revision engraved on each printed part. Bump a part's number whenever its geometry changes.
enum Revision {
    static let shell = 6        // r2: larger revision text, on the end wall; r3: un-mirrored (r1, r2 are mirror images); r4: corner screws, print aids; r5: 1.8 screw pilots; r6: seat chamfer, no print aids
    static let floor = 7        // r2: larger revision text; r3: un-mirrored; r4: corner screws, locating tabs, bridged counterbores; r5: 2.4 screw clearance; r6: clamp pads and rear stops on the connector bodies; r7: lead-in on the socket stops
    static let jig = 4          // r1: first Cadova jig; r2: larger passive pockets; r3: larger text; r4: un-mirrored
    static let terminator = 1
    static let mcuShell = 4     // r2: prop over the nano, view end walls, board-side tabs with wire gaps; r3: chambers for the switch and button; r4: board-side legs down to the floor
    static let mcuFloor = 3     // r2: clamp pads and rear stops on the socket bodies; r3: lead-in on the socket stops, tabs at the board-side legs
    static let coupon = 3       // r1: 13.7–14.2; r2: from 13.5; r3: larger text
    static func label(_ n: Int) -> String { "r\(n)" }
}

/// Engraving text on the xy plane, extruded 1 mm upward from z = 0. Engrave 0.6 deep.
func engraving(_ text: String, size: Double = 5.0) -> any Geometry3D {
    Text(text).withFont("DejaVu Sans", style: "Bold", size: size)
        .withTextAlignment(horizontal: .center, vertical: .center)
        .extruded(height: 1.0)
}

enum P {
    // Key module board (KeyModule.kicad_pcb frame, mm). x across the column, y along it.
    static let boardWidth = 16.85
    static let boardLength = 100.0
    static let boardThickness = 1.6
    static let boardOriginX = 70.55           // board frame x of the socket edge
    static let boardOriginY = 40.0
    static let keyX = 77.5
    static let keyYs = [51.93, 70.93, 89.86, 108.93, 128.0]
    static let rowYs = [58.1, 77.1, 115.1]    // connector rows

    // Connectors on the board's back (hanxia HX PM2.54 / PZ2.54). Measured 2026-09-26: both bodies flush with the
    // plane of their feet, so they sit on the board; 2.5 thick (datasheet; calipers read 2.43), axis 1.25 below it.
    // The header is soldered tilted by the joint angle (solder jig), pivoting about the inboard end of its pads.
    static let socketWidth = 7.87
    static let socketDepth = 8.5
    static let socketOverhang = 2.0           // mouth past the board edge
    static let headerWidth = 7.62
    static let headerBodyDepth = 2.5
    static let headerTailReach = 4.8          // body face to foot tip, along the board
    static let headerPadInboardEnd = 5.685    // foot tip from the board edge: pad row 4.1 in, pads 3.17 long
    static let headerFoot = 2.3               // straight end of the tail that lies on the pad
    static let socketTail = 3.2               // rear face to foot tip, along the board
    static let socketFoot = 1.3               // straight end of the tail that lies on the pad
    static let socketTailThickness = 0.4      // across the board; the tail is 0.64 along the row
    static let pinPitch = 2.54
    static let pinSquare = 0.64
    static let pinLength = 6.0
    static let bodyFloat = 0.0                // body to the board
    static let bodyHeight = 2.5
    static let pinAxisBelowBoard = 1.25
    static let tailClearance = 0.3            // rear stops to the tails, which leave the rear face at the pin axis
    static let rearStopThickness = 1.2        // along the pin axis
    static let rearStopLeadIn = 0.3           // 45° chamfer on the socket stop's tip; more than boardClearance
    static let stakingRoom = 0.5              // kept free beside each body's long sides, for the glue that stakes it to the board
    static let jointAngle = 8.0°
    /// Where the neighbour's socket mouth lands on this module's header axis, past the board edge (untilted).
    /// 2.0 keeps the 4.0 mm board-to-board convention: 0.39 mm between header body and socket mouth,
    /// 5.6 mm pin insertion.
    static let mouthPastHeaderEdge = 2.0

    // Switch and plate
    static let switchCutout = 13.7            // coupon 2026-09-25: 13.7 clicks snug on this printer; Kailh draws 13.8
    static let seatChamfer = 0.4              // 45° on the opening's top edge; leaves the top housing 0.25 of seat per side
    static let switchFlange = 15.3            // Choc v1 top housing is 15.0; pocket the plate top is sunk into
    static let plateThickness = 1.3
    static let plateToBoard = 2.2             // plate top to board top, Choc v1 plate mount

    // Shell
    static let wall = 1.8                     // side walls at the board plane
    static let endWall = 6.6                  // guide pin, its hole and the spring flap; corner screws behind them
    static let boardClearance = 0.2
    static let floorThickness = 1.5
    static let componentClearance = 0.3       // lowest component to the floor's top
    static let ledgeWidth = 1.0
    static let ledgeLength = 2.0              // end ledges, along y
    static let socketSideTabYs = [61.4, 99.4, 118.5]   // gaps between switch housings, clear of R4
    static let pillarXs = [6.0, 12.0]         // floor pillars under the board's bare end margins
    static let pillarYs = [41.85, 137.5]      // board frame
    static let pillarDiameter = 3.0
    static let screwXs = [2.8, 18.0]          // vertical M2s in the end walls' corners
    static let screwY = 4.9                   // from the outer end face, behind the guide hole
    static let tabHeight = 1.0                // floor locating tabs, above the floor's top; clear of the Choc legs
    static let tabThickness = 1.2
    static let tabClearance = 0.1
    static let sideTabYs: [(Double, Double)] = [(44, 52), (63.5, 72), (84, 108), (121, 134)]  // board frame, clear of the connector rows
    static let endTabXs: [(Double, Double)] = [(2.2, 4.2), (14.2, 17.2)]                    // clear of the pillars
    static let layer = 0.2                    // print layer: bridge steps in the floor's counterbores
    static let screwHole = 1.8                // M2 self-tapping; printed holes come out ~0.2 small, 1.6 was too tight
    static let screwDepth = 5.0
    static let screwClearance = 2.4
    static let screwHeadDiameter = 4.0
    static let screwHeadDepth = 0.8

    // Joint: a guide pin at each end of the header face enters a hole in the neighbour's end wall before the
    // connector pins reach their sockets; the end wall's outer skin over the hole is a spring flap whose tooth
    // clicks into a notch in the pin.
    static let guideWidth = 2.2               // along y, flat sides
    static let guideFlat = 1.4                // height of the flat sides; 45° gables above and below
    static let guideReach = 11.3              // past the header face; connector pins reach 5.6
    static let guideRoot = 2.5                // embedded in this module's end wall
    static let guideTaper = 1.0
    static let guideTipScale = 0.45
    static let guideZ = 0.2                   // centre, neighbour frame (board back = 0)
    static let guideClearance = 0.1
    static let flapThickness = 1.0            // outer skin of the end wall
    static let flapSlit = 0.3                 // behind the flap
    static let flapHinge = 0.6                // hinge line from the socket face, x
    static let flapFreeEdge = 10.8
    static let flapSlot = 0.6                 // at the free edge; fingernail room for the 90° catch
    static let toothX = 9.2                   // tooth centre, x: 8.6 from the hinge keeps flap strain near 1.4 %
    static let toothBase = 2.0
    static let toothEngagement = 0.6          // into the guide pin's notch
    static let toothCatchAngle = 45.0°        // 45 pulls apart by hand; 90 locks until the flap is pried out

    // Terminator module: headers embedded at a print pause, and lugs on free faces
    static let embedClearance = 0.15          // header body and pins in their pockets
    static let footClearance = 0.6            // feet, solder and the wire between them
    static let wireDiameter = 1.3             // insulated, under the feet
    static let terminatorWall = 1.6           // behind the feet, to the free face
    static let lugReach = 10.0                // out from the free face
    static let lugLength = 30.0               // along the column, flush with each column end
    static let lugWall = 3.0                  // around the strap slot
    static let lugRounding = 2.0              // outer vertical edges

    // MCU module. nice!nano v2 measured 2026-09-26; other parts from their vendors' drawings (Case design.md).
    static let usbEnd = ColumnEnd.sw5         // the nice!nano's end; the battery lies at the other
    static let nanoWidth = 18.15
    static let nanoLength = 33.4              // board, without the USB-C port
    static let nanoHeight = 2.75              // tallest part but the port, from the board's underside
    static let portOverhang = 0.86            // 34.26 overall
    static let portWidth = 8.94
    static let portHeight = 3.3
    static let portDepth = 7.35               // into the board, for the mock-up only
    static let nanoRise = 2.0                 // on a rib, clear of wires soldered underneath
    static let batteryWidth = 30.0            // electrokit 41016063, 750 mAh
    static let batteryLength = 48.0
    static let batteryThickness = 5.0
    static let batterySlack = 0.5             // each side, and above: room to swell
    static let viewWidth = 14.0               // nice!view, header along one short edge
    static let viewLength = 36.0
    static let viewBackParts = 1.0            // below its 1.0 PCB
    static let viewPCB = 1.0
    static let viewGlass = 0.9
    static let viewActiveInset = (header: 4.6, far: 6.1, side: 1.6)
    static let viewAir = 0.2                  // over the socket board
    static let viewLip = 0.6                  // plate over the glass's edges
    static let windowMargin = 0.4             // window past the active area
    static let switchBody = (u: 2.6, y: 6.7, h: 1.4)       // Alps SSSS811101, slider toward the free face
    static let switchSlider = (u: 1.5, y: 1.3, h: 1.1, travel: 1.5)
    static let resetBody = (u: 3.5, y: 4.7, h: 1.65)       // Panasonic EVQPUC02K, side-operated
    static let resetActuator = (u: 1.0, y: 2.6, h: 1.4)    // hangs from the body's top face
    static let switchSpan = 7.7               // with its ground terminals, which stick out 0.5 at each end
    static let switchTails = 1.2              // signal terminals past the body's back
    static let resetSpan = 6.4                // terminal tip to tip, two at each end
    static let chamberWall = 1.2              // around the switch and the button, so they bear the press
    static let jackBody = (u: 5.9, y: 7.6, h: 4.8)         // JST S2B-PH-K-S, mating face toward the battery
    static let jackPins = 6.25                // from the mating face
    static let jackTails = 3.4
    static let jackSeat = 2.0
    static let freeWallPocket = (switch: 0.8, reset: 0.6)  // how far each body sits into the free wall
    static let mcuPlate = 1.4                 // a whole number of layers: the plate is parallel to the top the shell prints on
    static let bayEndWall = 1.6               // end walls beside the socket strip's
    static let outerScrewInset = (x: 2.6, y: 4.1)          // from the free wall's inner face and the end face
    static let bossDiameter = 5.6
    /// Board-side tabs (module y): each a ledge over the socket board's edge and a leg beside it. Wires from the view
    /// and the board's pads cross the edge between them, clear of the connector rows.
    static let boardSideTabYs: [(Double, Double)] = [(12, 16), (42, 46), (55, 59), (98, 102)]

    // Derived
    static var pocketWidth: Double { boardWidth + 2 * boardClearance }
    static var pocketLength: Double { boardLength + 2 * boardClearance }
    static var outerLength: Double { pocketLength + 2 * endWall }
    static var boardTopZ: Double { boardThickness }
    static var ceilingZ: Double { plateTopZ - plateThickness }
    static var plateTopZ: Double { boardTopZ + plateToBoard }
    static var pinAxisZ: Double { -pinAxisBelowBoard }
    static var guideY: Double { flapThickness + guideClearance + guideWidth / 2 }
}
