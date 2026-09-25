import Cadova

await Project(packageRelative: "../../build/roamy-v4") {
    await Model("key-module") {
        KeyModuleShell().inPart(name: "Shell")
        KeyModuleFloor().inPart(name: "Floor", color: .blue)
        BoardMockup().inPart(name: "Board")
    }
    await Model("key-module-print") {
        // shell on its socket-wall side: the levers stand up, nothing needs support
        KeyModuleShell().rotated(y: -90°).translated(x: P.height).inPart(name: "Shell")
        KeyModuleFloor().translated(x: P.height + 10).inPart(name: "Floor")
    }
    await Model("choc-cutout-coupon") { ChocCutoutCoupon() }
    await Model("two-modules") {
        KeyModuleShell().inPart(name: "Left shell")
        KeyModuleFloor().inPart(name: "Left floor", color: .blue)
        KeyModuleShell().transformed(Frame.neighbour).inPart(name: "Right shell", color: .orange)
        BoardMockup().inPart(name: "Left board")
    }
}

// STL copies of the parts for headless checks; the viewer uses the 3MF files above.
await Project(packageRelative: "../../build/roamy-v4/check") {
    await Model("shell", options: .format3D(.stl)) { KeyModuleShell() }
    await Model("floor", options: .format3D(.stl)) { KeyModuleFloor() }
    await Model("board", options: .format3D(.stl)) { BoardMockup() }
    await Model("right-shell", options: .format3D(.stl)) { KeyModuleShell().transformed(Frame.neighbour) }
}

await Project(packageRelative: "../../build/roamy-v4/check") {
    await Model("dbg-channel", options: .format3D(.stl)) { JointLevers.channel(index: 0) }
    await Model("dbg-lever", options: .format3D(.stl)) { JointLevers.lever(index: 0) }
}
