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

    // Connectors on the board's underside (hanxia HX PM2.54 / PZ2.54)
    static let socketWidth = 7.87
    static let socketHeight = 2.5
    static let socketOverhang = 2.0           // mouth past the board edge, flush with the wall face
    static let headerWidth = 7.62
    static let pinPitch = 2.54
    static let pinSquare = 0.64
    static let pinLength = 6.0
    static let pinAxisBelowBoard = 1.25       // pin centreline below the board's back surface
    static let jointAngle = 8.0°               // between neighbouring modules; header pins leave at this angle

    // Switch and plate
    static let switchCutout = 13.9            // Choc v1 body is 13.8; clips grip a 1.3 plate
    static let plateThickness = 1.3
    static let plateToBoard = 2.2             // plate top to board top, Choc v1 plate mount

    // Shell
    static let wall = 1.8
    static let endWall = 2.0
    static let boardClearance = 0.2
    static let floorThickness = 1.5
    static let cavityBelowBoard = 3.6         // 8-degree header body floats 3.3 below the board
    static let ledgeWidth = 1.0
    static let ledgeLength = 2.0              // end ledges, along y
    static let postRadius = 2.0               // D-posts on the socket wall near each end
    static let postY = 7.3                    // from the outer end face; clear of the lever windows and the SW5 socket
    static let headerPostYs = [64.1, 121.1]   // board y; the header wall is bare between a header body and the next +3.3V pad
    static let headerPostDepth = 2.5          // into the cavity
    static let headerPostWidth = 3.0
    static let headerScrewInset = 0.4         // screw centre inside the wall's inner face, the outer face leans away
    static let screwHole = 1.6                // M2 self-tapping
    static let screwDepth = 5.0
    static let screwClearance = 2.2
    static let screwHeadDiameter = 4.0
    static let screwHeadDepth = 0.8
    static let socketSideTabYs = [61.4, 99.4, 118.5]   // gaps between switch housings, clear of R4

    // Joint levers: a rigid beam per module end, carried by a spring panel in the end wall
    static let leverThickness = 1.2           // along y
    static let leverHeight = 2.0
    static let leverReach = 3.5               // past this module's header face; the barb lives here
    static let leverBarb = 0.5
    static let leverBarbRamp = 1.0
    static let leverCatchAngle = 90.0°         // 90 locks; about 60 pulls apart by hand
    static let leverInset = 2.0               // from the board end, along y; clear of the SW5 hotswap socket
    static let leverZ = 2.0                   // underside above the module's underside
    static let leverTravel = 0.8              // sideways travel that frees the barb
    static let leverClearance = 0.3
    static let panelThickness = 1.0           // end-wall spring panel, thinned from the inside
    static let panelSlot = 0.4
    static let panelX0 = 2.6                  // hinge line, near the socket-wall corner
    static let panelX1 = 18.6                 // free end, just short of the header wall
    static let panelZ0 = 1.9
    static let panelZ1 = 4.7
    static let leverRootX = 17.0              // where the lever leaves the panel, module x

    // Derived
    static var pocketWidth: Double { boardWidth + 2 * boardClearance }
    static var pocketLength: Double { boardLength + 2 * boardClearance }
    static var outerWidth: Double { pocketWidth + 2 * wall }            // column pitch at the board plane
    static var outerLength: Double { pocketLength + 2 * endWall }
    static var boardBottomZ: Double { floorThickness + cavityBelowBoard }
    static var boardTopZ: Double { boardBottomZ + boardThickness }
    static var height: Double { boardTopZ + plateToBoard }
    static var pinAxisZ: Double { boardBottomZ - pinAxisBelowBoard }
}
