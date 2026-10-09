import Foundation

/// A DFU zip that adafruit-nrfutil made with `dfu genpkg --application`: an application image,
/// its init packet and `manifest.json`. See docs/firmware.md, "Updating over Bluetooth".
public struct DFUZip: Sendable {
    /// The zip's file name, which names the half in messages, for example "roamyboard_left.zip".
    public let name: String
    public let imageType: ImageType
    /// The image to write, `zmk.bin` in our builds.
    public let image: [UInt8]
    public let imageFileName: String
    public let initPacket: InitPacket
    public let initPacketFileName: String

    /// The sizes sent after Start DFU.
    public var imageSizes: ImageSizes { ImageSizes(application: UInt32(image.count)) }

    /// Reads and checks a DFU zip.
    /// - Parameter url: A `.zip` file, or a directory that holds exactly one, such as one
    ///   artifact that `gh run download` fetched.
    /// - Throws: `DFUZipError` naming the file and what is wrong with it.
    public init(contentsOf url: URL) throws(DFUZipError) {
        let file = try Self.resolve(url)
        let data: Data
        do { data = try Data(contentsOf: file) } catch { throw .unreadable(file.path, error.localizedDescription) }
        try self.init(name: file.lastPathComponent, zipData: data)
    }

    /// Parses and checks a DFU zip held in memory.
    /// - Parameter name: The name used in messages, usually the file name.
    /// - Throws: `DFUZipError` naming `name` and what is wrong with the zip.
    public init(name: String, zipData: Data) throws(DFUZipError) {
        self.name = name
        let archive: ZipArchive
        do { archive = try ZipArchive(zipData) } catch { throw .invalid(name, error.description) }
        guard let manifestData = archive.entries["manifest.json"] else { throw .invalid(name, "it has no manifest.json") }
        let manifest: Manifest
        do { manifest = try JSONDecoder().decode(ManifestFile.self, from: manifestData).manifest } catch {
            throw .invalid(name, "manifest.json does not parse: \(error)")
        }
        for (key, value) in [("softdevice", manifest.softdevice), ("bootloader", manifest.bootloader), ("softdevice_bootloader", manifest.softdevice_bootloader)] where value != nil {
            throw .invalid(name, "its manifest has a \(key) image; roamy-ota sends application images only")
        }
        guard let application = manifest.application else { throw .invalid(name, "its manifest has no application image") }
        guard let bin = archive.entries[application.bin_file] else { throw .invalid(name, "it lacks \(application.bin_file), which the manifest names") }
        guard let dat = archive.entries[application.dat_file] else { throw .invalid(name, "it lacks \(application.dat_file), which the manifest names") }

        imageType = .application
        image = [UInt8](bin)
        imageFileName = application.bin_file
        initPacketFileName = application.dat_file
        do { initPacket = try InitPacket(bytes: [UInt8](dat)) } catch {
            throw .invalid(name, "\(application.dat_file) is not an init packet: \(error)")
        }

        guard !image.isEmpty, image.count % 4 == 0 else {
            throw .invalid(name, "\(imageFileName) is \(image.count) bytes; the bootloader takes only a non-empty whole number of 4-byte words")
        }
        guard initPacket.deviceType == InitPacket.adafruitDeviceType else {
            throw .invalid(name, String(format: "its init packet has device type 0x%04X; the Adafruit bootloader takes only 0x0052 (adafruit-nrfutil --dev-type 0x0052)", initPacket.deviceType))
        }
        if let expected = initPacket.imageCRC16, crc16(image) != expected {
            throw .invalid(name, String(format: "%@ has CRC-16 0x%04X, but its init packet says 0x%04X; the bootloader would reject it after the upload", imageFileName, crc16(image), expected))
        }
    }

    private static func resolve(_ url: URL) throws(DFUZipError) -> URL {
        var isDirectory: ObjCBool = false
        guard FileManager.default.fileExists(atPath: url.path, isDirectory: &isDirectory) else { throw .notFound(url.path) }
        guard isDirectory.boolValue else { return url }
        let zips = ((try? FileManager.default.contentsOfDirectory(atPath: url.path)) ?? [])
            .filter { $0.hasSuffix(".zip") }.sorted()
        guard zips.count == 1 else { throw .ambiguousDirectory(url.path, zips) }
        return url.appendingPathComponent(zips[0])
    }
}

public enum DFUZipError: Error, Equatable, CustomStringConvertible {
    case notFound(String)
    case unreadable(String, String)
    /// A directory argument held zero or several zips; they are listed.
    case ambiguousDirectory(String, [String])
    /// The zip (first value) is unusable for the reason in the second value.
    case invalid(String, String)

    public var description: String {
        switch self {
        case .notFound(let path): "\(path) does not exist"
        case .unreadable(let path, let reason): "cannot read \(path): \(reason)"
        case .ambiguousDirectory(let path, []): "\(path) holds no .zip; pass a DFU zip or a directory that holds one"
        case .ambiguousDirectory(let path, let zips): "\(path) holds several zips (\(zips.joined(separator: ", "))); pass the one for each half"
        case .invalid(let name, let reason): "\(name) is not a usable DFU zip: \(reason)"
        }
    }
}

private struct ManifestFile: Decodable {
    let manifest: Manifest
}

// Keys as adafruit-nrfutil's manifest.py writes them.
private struct Manifest: Decodable {
    struct Image: Decodable {
        let bin_file: String
        let dat_file: String
    }
    let application: Image?
    let bootloader: Image?
    let softdevice: Image?
    let softdevice_bootloader: Image?
}
