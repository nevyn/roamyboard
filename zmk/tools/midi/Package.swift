// swift-tools-version: 6.0
// roamy-midi brings a roamyboard's BLE-MIDI service online in CoreMIDI and prints what it plays; docs/midi.md.
import PackageDescription

let package = Package(
    name: "roamy-midi",
    platforms: [.macOS(.v14)],
    products: [
        .executable(name: "roamy-midi", targets: ["roamy-midi"]),
    ],
    targets: [
        .executableTarget(
            name: "roamy-midi",
            exclude: ["Info.plist"],
            linkerSettings: [
                // macOS kills a CoreBluetooth client without NSBluetoothAlwaysUsageDescription, even a CLI.
                .unsafeFlags([
                    "-Xlinker", "-sectcreate",
                    "-Xlinker", "__TEXT",
                    "-Xlinker", "__info_plist",
                    "-Xlinker", "\(Context.packageDirectory)/Sources/roamy-midi/Info.plist",
                ]),
            ]
        ),
    ],
    swiftLanguageModes: [.v6]
)
