import Foundation
import Cadova

/// Numbers the case builds on. Board and connector figures come from electronics/Electronics.md;
/// switch figures from the Kailh Choc v1 plate-mount convention.
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

    // Connectors on the board's underside (hanxia HX PM2.54 / PZ2.54, electronics/datasheets). Both bodies
    // float above the board on S-shaped tails; the header is soldered tilted by the joint angle in the
    // solder jig, pivoting about the inboard end of its pads, so its pins leave at the joint angle.
    static let socketWidth = 7.87
    static let socketDepth = 8.5
    static let socketOverhang = 2.0           // mouth past the board edge, flush with the wall face
    static let headerWidth = 7.62
    static let headerBodyDepth = 2.5
    static let headerTailReach = 4.8          // body face to foot tip, along the board
    static let headerPadInboardEnd = 5.685    // foot tip from the board edge: pad row 4.1 in, pads 3.17 long
    static let pinPitch = 2.54
    static let pinSquare = 0.64
    static let pinLength = 6.0
    static let bodyFloat = 1.05               // body underside above the board (to be measured)
    static let bodyHeight = 2.5
    static let pinAxisBelowBoard = 2.3        // pin and bore centreline from the board's back surface, untilted
    static let jointAngle = 8.0°               // between neighbouring modules

    // Switch and plate
    static let switchCutout = 13.7            // coupon 2026-09-25: 13.7 clicks snug on this printer; Kailh draws 13.8
    static let plateThickness = 1.3
    static let plateToBoard = 2.2             // plate top to board top, Choc v1 plate mount

    // Shell
    static let wall = 1.8
    static let endWall = 4.0                  // thick enough for the vertical M2 screws in its corners
    static let boardClearance = 0.2
    static let floorThickness = 1.5
    static let cavityBelowBoard = 4.9         // the tilted header body reaches 4.53 below the board
    static let ledgeWidth = 1.0
    static let ledgeLength = 2.0              // end ledges, along y
    static let cornerScrewXs = [1.3, 19.0]    // vertical screws in the end walls' corner blocks
    static let cornerScrewY = 2.0             // from the outer end face
    static let pillarXs = [6.0, 11.0]         // floor pillars under the board's bare end margins, clear of both levers
    static let pillarY = 6.0                  // from the outer end face: under the board margin, clear of the levers and the SW5 socket
    static let pillarDiameter = 3.0
    static let screwHole = 1.6                // M2 self-tapping
    static let screwDepth = 5.0
    static let screwClearance = 2.2
    static let screwHeadDiameter = 4.0
    static let screwHeadDepth = 0.8
    static let socketSideTabYs = [61.4, 99.4, 118.5]   // gaps between switch housings, clear of R4

    // Joint levers: a rigid beam per module end, carried by a spring panel in the end wall
    static let leverThickness = 2.0           // along y
    static let leverHeight = 3.0
    static let leverReach = 5.0               // past this module's header face; the barb lives here
    static let leverBarb = 1.2                // sideways, the full lever height; catches 0.9 of the neighbour's wall
    static let leverBarbRamp = 2.0
    static let leverCatchAngle = 90.0°         // 90 locks; about 60 pulls apart by hand
    static let leverInset = 2.5               // from the board end, along y; clear of the corner screws and the SW5 hotswap socket
    static let leverZ = 1.8                   // underside above the module's underside; 0.3 above the floor top
    static let leverTravel = 1.5              // sideways travel that frees the barb
    static let leverClearance = 0.3
    static let panelThickness = 1.2           // end-wall spring panel, thinned from the inside
    static let panelSlot = 0.4
    static let panelX0 = 2.6                  // hinge line, near the socket-wall corner
    static let panelX1 = 17.6                 // free end, leaving the corner block for its screw
    static let panelZ0 = 1.7
    static let panelZ1 = 4.9
    static let leverRootX = 16.0              // where the lever leaves the panel, module x

    // Derived
    static var pocketWidth: Double { boardWidth + 2 * boardClearance }
    static var pocketLength: Double { boardLength + 2 * boardClearance }
    static var outerWidth: Double { pocketWidth + 2 * wall }            // column pitch at the board plane
    static var outerLength: Double { pocketLength + 2 * endWall }
    static var boardBottomZ: Double { floorThickness + cavityBelowBoard }
    static var boardTopZ: Double { boardBottomZ + boardThickness }
    static var height: Double { boardTopZ + plateToBoard }

    /// A point of the untilted header, (past the edge, below the board), after the solder-jig tilt.
    static func tiltedHeader(_ x: Double, _ z: Double) -> (x: Double, z: Double) {
        let a = jointAngle.radians, dx = x + headerPadInboardEnd
        return (-headerPadInboardEnd + dx * cos(a) - z * sin(a), dx * sin(a) + z * cos(a))
    }
    static var headerBodyX0: Double { headerPadInboardEnd - headerTailReach }   // untilted body face inside the edge
    /// Where the tilted pin line crosses the joint plane, below the board: the height the neighbour's bore must have there.
    static var jointAxisBelowBoard: Double {
        let p = tiltedHeader(-headerBodyX0 + headerBodyDepth, pinAxisBelowBoard)
        return p.z + (wall + boardClearance - p.x) * tan(jointAngle.radians)
    }
    static var jointAxisZ: Double { boardBottomZ - jointAxisBelowBoard }
    static var pinAxisZ: Double { boardBottomZ - pinAxisBelowBoard }
    /// The neighbour sits this much lower at the joint than a flat pin would put it.
    static var jointOffset: Double { jointAxisBelowBoard - pinAxisBelowBoard }
    /// Tilted header body extents, past the edge and below the board.
    static var headerBodyTilted: (xMin: Double, xMax: Double, zMin: Double, zMax: Double) {
        let c = [(-headerBodyX0, bodyFloat), (-headerBodyX0 + headerBodyDepth, bodyFloat),
                 (-headerBodyX0, bodyFloat + bodyHeight), (-headerBodyX0 + headerBodyDepth, bodyFloat + bodyHeight)].map { tiltedHeader($0.0, $0.1) }
        return (c.map(\.x).min()!, c.map(\.x).max()!, c.map(\.z).min()!, c.map(\.z).max()!)
    }
}
