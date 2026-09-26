import Foundation
import Cadova

print(String(format: "joint centre (%.2f, %.2f), skew %.2f°, height %.2f, top %.2f..%.2f, floor top %.2f..%.2f (z at socket face .. header face)",
             Joint.centre.x, Joint.centre.y, Joint.skew.degrees, Joint.top - Joint.bottom,
             Joint.corner(Joint.socketFace, Joint.top).y, Joint.corner(Joint.headerFace, Joint.top).y,
             Joint.corner(Joint.socketFace, Joint.rim).y, Joint.corner(Joint.headerFace, Joint.rim).y))

await Project(packageRelative: "../../build/roamy-v4") {
    await Model("key-module") {
        KeyModuleShell().inPart(name: "Shell")
        KeyModuleFloor().inPart(name: "Floor", color: .blue)
        BoardMockup().inPart(name: "Board")
    }
    await Model("key-module-print") {
        KeyModuleShell(printAids: true).transformed(PrintPose.shell).inPart(name: "Shell")
        KeyModuleFloor().transformed(PrintPose.floor).translated(x: 30).inPart(name: "Floor")
    }
    await Model("choc-cutout-coupon") { ChocCutoutCoupon() }
    await Model("solder-jig") {   // as used: turned over, board front down in the pocket
        SolderJig().transformed(SolderJig.printPose).inPart(name: "Jig")
        BoardMockup(switches: false).transformed(SolderJig.printPose).inPart(name: "Board")
    }
    await Model("solder-jig-print") { SolderJig().transformed(SolderJig.printPose) }
    await Model("three-modules") {
        KeyModuleShell().inPart(name: "Left shell")
        KeyModuleFloor().inPart(name: "Left floor", color: .blue)
        BoardMockup().inPart(name: "Left board")
        KeyModuleShell().transformed(Joint.neighbourTransform).inPart(name: "Middle shell", color: .orange)
        BoardMockup().transformed(Joint.neighbourTransform).inPart(name: "Middle board")
        KeyModuleShell().transformed(Joint.neighbourTransform.concatenated(with: Joint.neighbourTransform)).inPart(name: "Right shell")
    }
}

// STL copies of the parts for headless checks; the viewer uses the 3MF files above.
await Project(packageRelative: "../../build/roamy-v4/check") {
    await Model("shell", options: .format3D(.stl)) { KeyModuleShell() }
    await Model("floor", options: .format3D(.stl)) { KeyModuleFloor() }
    await Model("board", options: .format3D(.stl)) { BoardMockup() }
    await Model("bare-board", options: .format3D(.stl)) { BoardMockup(switches: false) }
    await Model("right-shell", options: .format3D(.stl)) { KeyModuleShell().transformed(Joint.neighbourTransform) }
    await Model("right-floor", options: .format3D(.stl)) { KeyModuleFloor().transformed(Joint.neighbourTransform) }
    await Model("right-board", options: .format3D(.stl)) { BoardMockup().transformed(Joint.neighbourTransform) }
    await Model("jig", options: .format3D(.stl)) { SolderJig() }
    await Model("jig-print", options: .format3D(.stl)) { SolderJig().transformed(SolderJig.printPose) }
    await Model("shell-print", options: .format3D(.stl)) { KeyModuleShell(printAids: true).transformed(PrintPose.shell) }
    await Model("floor-print", options: .format3D(.stl)) { KeyModuleFloor().transformed(PrintPose.floor) }
}
