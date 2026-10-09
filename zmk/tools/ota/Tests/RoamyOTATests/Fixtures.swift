import Foundation
@testable import RoamyOTA

/// DFU zips built in the test from known bytes, laid out as adafruit-nrfutil 0.5.3 lays them out.
enum Fixtures {
    /// An image of `size` bytes with a recognisable pattern.
    static func image(size: Int) -> [UInt8] {
        (0..<size).map { UInt8(truncatingIfNeeded: $0 &* 7 &+ $0 >> 8) }
    }

    static func initPacket(for image: [UInt8], deviceType: UInt16 = 0x0052, crc: UInt16? = nil) -> InitPacket {
        InitPacket(
            deviceType: deviceType, deviceRevision: 0xFFFF, applicationVersion: 0xFFFF_FFFF,
            softDeviceRequirements: [0xFFFE], extendedData: (crc ?? crc16(image)).littleEndianBytes)
    }

    static let manifest = """
        {
            "manifest": {
                "application": {
                    "bin_file": "zmk.bin",
                    "dat_file": "zmk.dat",
                    "init_packet_data": {
                        "application_version": 4294967295,
                        "device_revision": 65535,
                        "device_type": 82,
                        "firmware_crc16": 0,
                        "softdevice_req": [65534]
                    }
                },
                "dfu_version": 0.5
            }
        }
        """

    /// The files of a DFU zip for `image`.
    static func files(image: [UInt8], initPacket: InitPacket? = nil) -> [String: [UInt8]] {
        [
            "manifest.json": Array(manifest.utf8),
            "zmk.bin": image,
            "zmk.dat": (initPacket ?? Self.initPacket(for: image)).bytes,
        ]
    }

    /// Zips `files` with /usr/bin/zip, deflated unless `stored`.
    static func zip(_ files: [String: [UInt8]], stored: Bool = false) throws -> Data {
        let directory = try temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: directory) }
        for (name, bytes) in files {
            try Data(bytes).write(to: directory.appendingPathComponent(name))
        }
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/usr/bin/zip")
        process.currentDirectoryURL = directory
        process.arguments = ["-q", "-X", stored ? "-0" : "-9", "out.zip"] + files.keys.sorted()
        try process.run()
        process.waitUntilExit()
        guard process.terminationStatus == 0 else { throw FixtureError.zipFailed(process.terminationStatus) }
        return try Data(contentsOf: directory.appendingPathComponent("out.zip"))
    }

    static func dfuZip(imageSize: Int = 1000) throws -> DFUZip {
        try DFUZip(name: "roamyboard_left.zip", zipData: zip(files(image: image(size: imageSize))))
    }

    static func temporaryDirectory() throws -> URL {
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("roamy-ota-tests-\(UUID().uuidString)")
        try FileManager.default.createDirectory(at: url, withIntermediateDirectories: true)
        return url
    }

    enum FixtureError: Error { case zipFailed(Int32) }
}
