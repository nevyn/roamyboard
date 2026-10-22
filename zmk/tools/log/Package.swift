// swift-tools-version: 6.0
// roamy-log reads a roamyboard's logs over Bluetooth, from both halves; docs/logging.md.
import PackageDescription

let package = Package(
    name: "roamy-log",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "roamy-log", targets: ["roamy-log"]),
    ],
    targets: [
        // Frames, records, sequence tracking, output and options; no CoreBluetooth, so tests never touch TCC.
        .target(name: "RoamyLog"),
        .executableTarget(
            name: "roamy-log",
            dependencies: ["RoamyLog"],
            exclude: ["Info.plist"],
            linkerSettings: [
                // macOS kills a CoreBluetooth client without NSBluetoothAlwaysUsageDescription, even a CLI.
                .unsafeFlags([
                    "-Xlinker", "-sectcreate",
                    "-Xlinker", "__TEXT",
                    "-Xlinker", "__info_plist",
                    "-Xlinker", "\(Context.packageDirectory)/Sources/roamy-log/Info.plist",
                ]),
            ]
        ),
        .testTarget(name: "RoamyLogTests", dependencies: ["RoamyLog"], exclude: ["Fixtures"]),
    ],
    swiftLanguageModes: [.v6]
)
