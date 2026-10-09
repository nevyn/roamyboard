import Foundation
import Testing
@testable import RoamyOTA

@Suite struct DFUZipTests {
    @Test(arguments: [false, true])
    func readsImageAndInitPacket(stored: Bool) throws {
        let image = Fixtures.image(size: 1000)
        let zip = try DFUZip(name: "roamyboard_left.zip", zipData: Fixtures.zip(Fixtures.files(image: image), stored: stored))

        #expect(zip.name == "roamyboard_left.zip")
        #expect(zip.imageType == .application)
        #expect(zip.image == image)
        #expect(zip.imageFileName == "zmk.bin")
        #expect(zip.initPacketFileName == "zmk.dat")
        #expect(zip.initPacket.deviceType == 0x0052)
        #expect(zip.initPacket.softDeviceRequirements == [0xFFFE])
        #expect(zip.initPacket.imageCRC16 == crc16(image))
        #expect(zip.imageSizes == ImageSizes(application: 1000))
    }

    @Test func initPacketLayout() throws {
        let packet = InitPacket(deviceType: 0x0052, deviceRevision: 0xFFFF, applicationVersion: 0xFFFF_FFFF, softDeviceRequirements: [0xFFFE], extendedData: [0x34, 0x12])
        #expect(packet.bytes == [0x52, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, 0xFE, 0xFF, 0x34, 0x12])
        #expect(try InitPacket(bytes: packet.bytes) == packet)
        #expect(packet.imageCRC16 == 0x1234)
        #expect(throws: InitPacket.ParseError.tooShort(11)) { try InitPacket(bytes: [0x52, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0xFE]) }
    }

    @Test func checksums() {
        #expect(crc16(Array("123456789".utf8)) == 0x29B1)
        #expect(crc32(Array("123456789".utf8)) == 0xCBF4_3926)
    }

    @Test func rejectsWrongDeviceType() throws {
        let image = Fixtures.image(size: 1000)
        let files = Fixtures.files(image: image, initPacket: Fixtures.initPacket(for: image, deviceType: 0xFFFF))
        let error = #expect(throws: DFUZipError.self) { try DFUZip(name: "x.zip", zipData: Fixtures.zip(files)) }
        #expect(error?.description.contains("device type 0xFFFF") == true)
    }

    @Test func rejectsImageThatDoesNotMatchItsCRC() throws {
        let image = Fixtures.image(size: 1000)
        let files = Fixtures.files(image: image, initPacket: Fixtures.initPacket(for: image, crc: 0xBEEF))
        let error = #expect(throws: DFUZipError.self) { try DFUZip(name: "x.zip", zipData: Fixtures.zip(files)) }
        #expect(error?.description.contains("0xBEEF") == true)
    }

    @Test func rejectsImageOfPartialWords() throws {
        let error = #expect(throws: DFUZipError.self) {
            try DFUZip(name: "x.zip", zipData: Fixtures.zip(Fixtures.files(image: Fixtures.image(size: 1001))))
        }
        #expect(error?.description.contains("1001 bytes") == true)
    }

    @Test func rejectsZipWithoutManifest() throws {
        var files = Fixtures.files(image: Fixtures.image(size: 8))
        files["manifest.json"] = nil
        #expect(throws: DFUZipError.invalid("x.zip", "it has no manifest.json")) { try DFUZip(name: "x.zip", zipData: Fixtures.zip(files)) }
    }

    @Test func rejectsCorruptEntry() throws {
        var data = try Fixtures.zip(Fixtures.files(image: Fixtures.image(size: 64)), stored: true)
        let marker = Data(Fixtures.image(size: 64).prefix(16))
        let range = try #require(data.range(of: marker))
        data[range.lowerBound] ^= 0xFF
        let error = #expect(throws: DFUZipError.self) { try DFUZip(name: "x.zip", zipData: data) }
        #expect(error?.description.contains("zmk.bin fails its CRC-32 check") == true)
    }

    @Test func directoryWithOneZipStandsForIt() throws {
        let directory = try Fixtures.temporaryDirectory()
        defer { try? FileManager.default.removeItem(at: directory) }
        try Fixtures.zip(Fixtures.files(image: Fixtures.image(size: 40))).write(to: directory.appendingPathComponent("roamyboard_right.zip"))
        try Data().write(to: directory.appendingPathComponent("roamyboard_right.uf2"))

        #expect(try DFUZip(contentsOf: directory).name == "roamyboard_right.zip")

        try Data().write(to: directory.appendingPathComponent("roamyboard_left.zip"))
        #expect(throws: DFUZipError.ambiguousDirectory(directory.path, ["roamyboard_left.zip", "roamyboard_right.zip"])) {
            try DFUZip(contentsOf: directory)
        }
    }

    @Test func dryRunNamesWhatIsSent() throws {
        let zip = try Fixtures.dfuZip(imageSize: 1000)
        let summary = zip.summary(settings: DFUSettings())
        #expect(summary.contains("01 04, then sizes 00 00 00 00 00 00 00 00 E8 03 00 00"))
        #expect(summary.contains("zmk.dat, 14 bytes: 52 00 FF FF FF FF FF FF 01 00 FE FF"))
        #expect(summary.contains("50 packets of up to 20 bytes, a receipt every 8 packets (6 receipts)"))
    }

    @Test func leftHalfGoesLast() throws {
        let data = try Fixtures.zip(Fixtures.files(image: Fixtures.image(size: 100)))
        func named(_ names: String...) throws -> [DFUZip] { try names.map { try DFUZip(name: $0, zipData: data) } }

        let split = updateOrder(try named("roamyboard_left.zip", "roamyboard_right.zip"))
        #expect(split.zips.map(\.name) == ["roamyboard_right.zip", "roamyboard_left.zip"])
        #expect(split.reordered)

        let alreadyOrdered = updateOrder(try named("roamyboard_right.zip", "roamyboard_LEFT-ota.zip"))
        #expect(alreadyOrdered.zips.map(\.name) == ["roamyboard_right.zip", "roamyboard_LEFT-ota.zip"])
        #expect(!alreadyOrdered.reordered)

        let unibody = updateOrder(try named("roamyboard.zip"))
        #expect(unibody.zips.map(\.name) == ["roamyboard.zip"])
        #expect(!unibody.reordered)
    }
}
