// swift-tools-version: 6.0
// roamy-ota flashes a DFU zip onto a half in OTA mode over Bluetooth; docs/firmware.md.
import PackageDescription

let package = Package(
    name: "roamy-ota",
    platforms: [.macOS(.v14)],
    targets: [
        // The DFU protocol, DFU zip reading and option parsing; no CoreBluetooth, so tests never touch TCC.
        .target(name: "RoamyOTA"),
        .testTarget(name: "RoamyOTATests", dependencies: ["RoamyOTA"]),
    ],
    swiftLanguageModes: [.v6]
)
