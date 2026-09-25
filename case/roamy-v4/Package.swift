// swift-tools-version: 6.3
import PackageDescription

let package = Package(
    name: "roamy-v4",
    platforms: [.macOS(.v14)],
    dependencies: [
        .package(url: "https://github.com/tomasf/Cadova.git", .upToNextMinor(from: "0.10.0")),
    ],
    targets: [
        .executableTarget(
            name: "roamy-v4",
            dependencies: ["Cadova"],
            swiftSettings: [.interoperabilityMode(.Cxx)]
        ),
    ]
)
