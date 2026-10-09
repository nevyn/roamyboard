// swift-tools-version: 6.0
// roamy-ota flashes a DFU zip onto a half in OTA mode over Bluetooth; docs/firmware.md.
import PackageDescription

let package = Package(
    name: "roamy-ota",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "roamy-ota", targets: ["roamy-ota"]),
    ],
    targets: [
        // The DFU protocol, DFU zip reading and option parsing; no CoreBluetooth, so tests never touch TCC.
        .target(name: "RoamyOTA"),
        .executableTarget(
            name: "roamy-ota",
            dependencies: ["RoamyOTA"],
            exclude: ["Info.plist"],
            linkerSettings: [
                // macOS kills a CoreBluetooth client without NSBluetoothAlwaysUsageDescription, even a CLI.
                .unsafeFlags([
                    "-Xlinker", "-sectcreate",
                    "-Xlinker", "__TEXT",
                    "-Xlinker", "__info_plist",
                    "-Xlinker", "\(Context.packageDirectory)/Sources/roamy-ota/Info.plist",
                ]),
            ]
        ),
        .testTarget(name: "RoamyOTATests", dependencies: ["RoamyOTA"]),
    ],
    swiftLanguageModes: [.v6]
)
