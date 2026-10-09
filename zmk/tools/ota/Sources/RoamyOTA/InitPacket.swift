/// The init packet of a legacy DFU zip (`zmk.dat`), as adafruit-nrfutil writes it with DFU
/// version 0.5: little-endian fields, then extended data that holds the image's CRC-16.
public struct InitPacket: Equatable, Sendable {
    /// The device type that the image is for. The Adafruit bootloader takes only 0x0052.
    public var deviceType: UInt16
    public var deviceRevision: UInt16
    public var applicationVersion: UInt32
    /// The SoftDevice firmware IDs that the image runs on; 0xFFFE means any.
    public var softDeviceRequirements: [UInt16]
    /// The bytes after the SoftDevice list. Two bytes are the image's CRC-16 (DFU version 0.5).
    public var extendedData: [UInt8]

    /// The device type that the Adafruit nRF52 bootloader requires.
    public static let adafruitDeviceType: UInt16 = 0x0052

    public init(
        deviceType: UInt16, deviceRevision: UInt16, applicationVersion: UInt32,
        softDeviceRequirements: [UInt16], extendedData: [UInt8]
    ) {
        self.deviceType = deviceType
        self.deviceRevision = deviceRevision
        self.applicationVersion = applicationVersion
        self.softDeviceRequirements = softDeviceRequirements
        self.extendedData = extendedData
    }

    /// Parses an init packet.
    /// - Throws: `InitPacket.ParseError` if `bytes` end before the SoftDevice list does.
    public init(bytes: [UInt8]) throws(ParseError) {
        guard bytes.count >= 10 else { throw .tooShort(bytes.count) }
        let count = Int(UInt16(littleEndianBytes: bytes[8..<10]))
        let listEnd = 10 + 2 * count
        guard bytes.count >= listEnd else { throw .tooShort(bytes.count) }
        deviceType = UInt16(littleEndianBytes: bytes[0..<2])
        deviceRevision = UInt16(littleEndianBytes: bytes[2..<4])
        applicationVersion = UInt32(littleEndianBytes: bytes[4..<8])
        softDeviceRequirements = stride(from: 10, to: listEnd, by: 2).map { UInt16(littleEndianBytes: bytes[$0..<$0 + 2]) }
        extendedData = Array(bytes[listEnd...])
    }

    public enum ParseError: Error, Equatable {
        /// The init packet has only this many bytes.
        case tooShort(Int)
    }

    /// The bytes sent to the bootloader.
    public var bytes: [UInt8] {
        deviceType.littleEndianBytes + deviceRevision.littleEndianBytes + applicationVersion.littleEndianBytes
            + UInt16(softDeviceRequirements.count).littleEndianBytes
            + softDeviceRequirements.flatMap(\.littleEndianBytes) + extendedData
    }

    /// The image CRC-16 that the bootloader checks on Validate Firmware; nil unless the extended
    /// data is exactly a CRC-16.
    public var imageCRC16: UInt16? {
        extendedData.count == 2 ? UInt16(littleEndianBytes: extendedData[0..<2]) : nil
    }
}

/// CRC-16/CCITT-FALSE (polynomial 0x1021, initial value 0xFFFF), the CRC that adafruit-nrfutil
/// puts in the init packet and that the bootloader recomputes over the received image.
public func crc16(_ bytes: some Sequence<UInt8>) -> UInt16 {
    var crc: UInt16 = 0xFFFF
    for byte in bytes {
        crc = (crc >> 8) | (crc << 8)
        crc ^= UInt16(byte)
        crc ^= (crc & 0xFF) >> 4
        crc ^= crc << 12
        crc ^= (crc & 0xFF) << 5
    }
    return crc
}
