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
    static let floorThickness = 1.7
    static let cavityBelowBoard = 4.5         // room for 8-degree headers, sockets and hooks
    static let ledgeWidth = 1.0
    static let ledgeLength = 2.0              // end ledges, along y
    static let postDiameter = 3.6
    static let screwHole = 1.6                // M2 self-tapping

    // Joint hooks, under the board at the module ends
    static let hookThickness = 1.2            // along y; the beam flexes this way
    static let hookHeight = 2.0
    static let hookReach = 8.0                // past this module's header face
    static let hookBarb = 0.6
    static let hookInset = 2.0                // from the board end, along y; clear of the SW5 hotswap socket
    static let hookClearance = 0.3
    static let hookBarbRamp = 1.2
    static let hookZ = 2.0                    // hook underside above the floor's underside
    static let postInset = 1.5                // post centre inside the end wall's inner face
    static let postFractions = [0.55, 0.85]   // across the pocket; clear of the previous module's hooks
    static let screwDepth = 5.0
    static let screwClearance = 2.2
    static let screwHeadDiameter = 4.0
    static let screwHeadDepth = 1.0
    static let socketSideTabYs = [61.4, 99.4, 118.5]   // gaps between switch housings, clear of R4

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
