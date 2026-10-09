import Compression
import Foundation

/// Reads the entries of a small zip file in memory: stored or deflated entries, no zip64, no
/// encryption. That covers what adafruit-nrfutil writes. Reading in-process, rather than with
/// /usr/bin/unzip, lets a broken zip fail with an error that names the entry.
struct ZipArchive {
    /// Entry contents by name, as listed in the central directory.
    let entries: [String: Data]

    enum ReadError: Error, Equatable, CustomStringConvertible {
        case notAZip
        case truncated(entry: String?)
        case unsupportedMethod(entry: String, method: UInt16)
        case inflateFailed(entry: String)
        case checksumMismatch(entry: String)

        var description: String {
            switch self {
            case .notAZip: "it is not a zip file (no end of central directory record)"
            case .truncated(let entry?): "it ends inside \(entry)"
            case .truncated(nil): "it ends inside the central directory"
            case .unsupportedMethod(let entry, let method): "\(entry) uses compression method \(method); only stored (0) and deflate (8) are supported"
            case .inflateFailed(let entry): "\(entry) does not inflate"
            case .checksumMismatch(let entry): "\(entry) fails its CRC-32 check; the zip is corrupt"
            }
        }
    }

    init(_ data: Data) throws(ReadError) {
        let bytes = [UInt8](data)
        guard let end = Self.endOfCentralDirectory(in: bytes) else { throw .notAZip }
        let count = Int(bytes.u16(end + 10))
        var offset = Int(bytes.u32(end + 16))
        var entries: [String: Data] = [:]
        for _ in 0..<count {
            guard offset + 46 <= bytes.count, bytes.u32(offset) == 0x0201_4B50 else { throw .truncated(entry: nil) }
            let method = bytes.u16(offset + 10)
            let crc = bytes.u32(offset + 16)
            let compressedSize = Int(bytes.u32(offset + 20))
            let size = Int(bytes.u32(offset + 24))
            let nameLength = Int(bytes.u16(offset + 28))
            let extraLength = Int(bytes.u16(offset + 30))
            let commentLength = Int(bytes.u16(offset + 32))
            let localHeader = Int(bytes.u32(offset + 42))
            guard offset + 46 + nameLength <= bytes.count else { throw .truncated(entry: nil) }
            let name = String(decoding: bytes[offset + 46..<offset + 46 + nameLength], as: UTF8.self)
            offset += 46 + nameLength + extraLength + commentLength

            guard localHeader + 30 <= bytes.count, bytes.u32(localHeader) == 0x0403_4B50 else { throw .truncated(entry: name) }
            let start = localHeader + 30 + Int(bytes.u16(localHeader + 26)) + Int(bytes.u16(localHeader + 28))
            guard start + compressedSize <= bytes.count else { throw .truncated(entry: name) }
            let stored = Array(bytes[start..<start + compressedSize])
            let contents: [UInt8]
            switch method {
            case 0: contents = stored
            case 8: contents = try Self.inflate(stored, size: size, entry: name)
            default: throw .unsupportedMethod(entry: name, method: method)
            }
            guard crc32(contents) == crc else { throw .checksumMismatch(entry: name) }
            entries[name] = Data(contents)
        }
        self.entries = entries
    }

    private static func endOfCentralDirectory(in bytes: [UInt8]) -> Int? {
        guard bytes.count >= 22 else { return nil }
        let lowest = max(0, bytes.count - 22 - 0xFFFF)
        return stride(from: bytes.count - 22, through: lowest, by: -1).first { bytes.u32($0) == 0x0605_4B50 }
    }

    private static func inflate(_ input: [UInt8], size: Int, entry: String) throws(ReadError) -> [UInt8] {
        if size == 0 { return [] }
        var output = [UInt8](repeating: 0, count: size)
        // COMPRESSION_ZLIB is raw DEFLATE (RFC 1951), which is what zip stores.
        let written = compression_decode_buffer(&output, size, input, input.count, nil, COMPRESSION_ZLIB)
        guard written == size else { throw .inflateFailed(entry: entry) }
        return output
    }
}

/// CRC-32 (IEEE 802.3), as zip uses it.
func crc32(_ bytes: [UInt8]) -> UInt32 {
    var crc: UInt32 = 0xFFFF_FFFF
    for byte in bytes {
        crc ^= UInt32(byte)
        for _ in 0..<8 { crc = crc & 1 == 1 ? (crc >> 1) ^ 0xEDB8_8320 : crc >> 1 }
    }
    return ~crc
}

extension [UInt8] {
    fileprivate func u16(_ i: Int) -> UInt16 { UInt16(self[i]) | UInt16(self[i + 1]) << 8 }
    fileprivate func u32(_ i: Int) -> UInt32 { UInt32(u16(i)) | UInt32(u16(i + 2)) << 16 }
}
