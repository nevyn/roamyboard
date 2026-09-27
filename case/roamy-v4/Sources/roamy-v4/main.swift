import Foundation
import Cadova

print(String(format: "joint centre (%.2f, %.2f), skew %.2f°, height %.2f, top %.2f..%.2f, floor top %.2f..%.2f (z at socket face .. header face)",
             Joint.centre.x, Joint.centre.y, Joint.skew.degrees, Joint.top - Joint.bottom,
             Joint.corner(Joint.socketFace, Joint.top).y, Joint.corner(Joint.headerFace, Joint.top).y,
             Joint.corner(Joint.socketFace, Joint.rim).y, Joint.corner(Joint.headerFace, Joint.rim).y))
print(String(format: "MCU module: top %.2f above the key modules' (view), %.2f thick; free face at x %.2f",
             MCU.top - Joint.top, MCU.top - Joint.bottom, MCU.freeFaceX))
print(String(format: "terminator: pause at %.2f mm print height, %.2f mm printed over the pockets",
             TerminatorModule.pauseHeight, TerminatorModule.pauseLevel - Joint.bottom))

await Project(packageRelative: "../../build/roamy-v4") {
    await Model("key-module") {
        KeyModuleShell().inPart(name: "Shell")
        KeyModuleFloor().inPart(name: "Floor", color: .blue)
        BoardMockup().inPart(name: "Board")
    }
    await Model("key-module-print") {
        KeyModuleShell().transformed(PrintPose.shell).inPart(name: "Shell")
        KeyModuleFloor().transformed(PrintPose.floor).translated(x: 30).inPart(name: "Floor")
    }
    await Model("choc-cutout-coupon") { ChocCutoutCoupon() }
    await Model("solder-jig") {   // as used: turned over, board front down in the pocket
        SolderJig().transformed(SolderJig.printPose).inPart(name: "Jig")
        BoardMockup(switches: false).transformed(SolderJig.printPose).inPart(name: "Board")
    }
    await Model("solder-jig-print") { SolderJig().transformed(SolderJig.printPose) }
    await Model("terminator") {   // with its neighbour, the first key module
        TerminatorModule().inPart(name: "Terminator")
        TerminatorEmbedded().inPart(name: "Headers", color: .orange)
        KeyModuleShell().transformed(Joint.neighbourTransform).inPart(name: "Key module shell", color: .gray)
        BoardMockup().transformed(Joint.neighbourTransform).inPart(name: "Key module board")
    }
    await Model("terminator-print") { TerminatorModule().transformed(PrintPose.shell) }
    await Model("mcu-module") {   // with the last key module, whose neighbour it is
        let n = Joint.neighbourTransform
        MCUShell().transformed(n).inPart(name: "Shell")
        MCUFloor().transformed(n).inPart(name: "Floor", color: .blue)
        MCUParts.socketBoard.transformed(n).inPart(name: "Socket board")
        MCUParts.all.transformed(n).inPart(name: "Parts", color: .orange)
        KeyModuleShell().inPart(name: "Key module shell", color: .gray)
        BoardMockup().inPart(name: "Key module board")
    }
    await Model("mcu-module-print") {
        MCUShell().transformed(PrintPose.topDown(top: MCU.top)).inPart(name: "Shell")
        MCUFloor().transformed(PrintPose.floor).translated(x: 70).inPart(name: "Floor")
    }
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
    await Model("shell-print", options: .format3D(.stl)) { KeyModuleShell().transformed(PrintPose.shell) }
    // parts that the slicer supports on purpose (tools/overhang.py): the joint parts, which include the guide pins; the
    // terminator's below
    await Model("shell-print-painted-support", options: .format3D(.stl)) {
        for joint in KeyModuleShell.joints { joint.added.transformed(PrintPose.shell) }
    }
    await Model("floor-print", options: .format3D(.stl)) { KeyModuleFloor().transformed(PrintPose.floor) }
    await Model("terminator", options: .format3D(.stl)) { TerminatorModule() }
    await Model("terminator-outline", options: .format3D(.stl)) { TerminatorModule.outline }
    await Model("terminator-embedded", options: .format3D(.stl)) { TerminatorEmbedded() }
    await Model("terminator-print", options: .format3D(.stl)) { TerminatorModule().transformed(PrintPose.shell) }
    await Model("terminator-print-painted-support", options: .format3D(.stl)) {
        TerminatorModule.joint.added.transformed(PrintPose.shell)
    }
    await Model("terminator-embedded-print", options: .format3D(.stl)) { TerminatorEmbedded().transformed(PrintPose.shell) }
    await Model("mcu-shell", options: .format3D(.stl)) { MCUShell() }
    await Model("mcu-floor", options: .format3D(.stl)) { MCUFloor() }
    await Model("mcu-outline", options: .format3D(.stl)) { alongColumn(MCU.section(from: Joint.rim, to: MCU.top)) }
    await Model("mcu-parts", options: .format3D(.stl)) { MCUParts.all }
    await Model("socket-board", options: .format3D(.stl)) { MCUParts.socketBoard }
    await Model("right-mcu-shell", options: .format3D(.stl)) { MCUShell().transformed(Joint.neighbourTransform) }
    await Model("right-mcu-floor", options: .format3D(.stl)) { MCUFloor().transformed(Joint.neighbourTransform) }
    await Model("right-socket-board", options: .format3D(.stl)) { MCUParts.socketBoard.transformed(Joint.neighbourTransform) }
    await Model("mcu-shell-print", options: .format3D(.stl)) { MCUShell().transformed(PrintPose.topDown(top: MCU.top)) }
    await Model("mcu-floor-print", options: .format3D(.stl)) { MCUFloor().transformed(PrintPose.floor) }
    for (i, shank) in MCUFixtures.shanks.enumerated() {
        await Model("mcu-shank-\(i)", options: .format3D(.stl)) { shank }
    }
    await Model("mcu-nano", options: .format3D(.stl)) { MCUParts.nano }
    await Model("mcu-view", options: .format3D(.stl)) { MCUParts.view }
    await Model("mcu-switch", options: .format3D(.stl)) { MCUParts.switchPart }
    await Model("mcu-reset", options: .format3D(.stl)) { MCUParts.resetPart }
    await Model("mcu-wire-paths", options: .format3D(.stl)) { MCUFixtures.wirePaths }
    await Model("mcu-tongue", options: .format3D(.stl)) { MCUParts.usbTongue }
    await Model("mcu-usb-plug", options: .format3D(.stl)) { MCUFixtures.plug }
    // the JointSide contract: what each side owns, cuts, adds and keeps clear
    await Model("shell-outline", options: .format3D(.stl)) { alongColumn(keystone(from: Joint.rim, to: Joint.top)) }
    for (name, joint) in [("header", HeaderSideJoint() as any JointSide), ("socket", SocketSideJoint())] {
        await Model("\(name)-joint-reserved", options: .format3D(.stl)) { joint.reserved }
        await Model("\(name)-joint-removed", options: .format3D(.stl)) { joint.removed }
        await Model("\(name)-joint-added", options: .format3D(.stl)) { joint.added }
        await Model("\(name)-joint-keep-out", options: .format3D(.stl)) { joint.keepOut }
    }
}
try String(format: "%.2f\n", TerminatorModule.pauseHeight)
    .write(to: URL(fileURLWithPath: #filePath).appending(path: "../../../../../build/roamy-v4/check/terminator-pause.txt").standardized,
           atomically: true, encoding: .utf8)
