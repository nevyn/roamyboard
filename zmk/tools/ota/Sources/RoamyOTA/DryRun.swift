import Foundation

extension DFUZip {
    /// What `LegacyDFUUpdate` would send for this zip with `settings`, for `--dry-run`.
    public func summary(settings: DFUSettings) -> String {
        let packets = (image.count + settings.packetSize - 1) / settings.packetSize
        let interval = Int(settings.packetReceiptInterval.packets)
        // No receipt follows the last packet; the final response does.
        let receipts = (packets - 1) / interval
        let crc = initPacket.imageCRC16.map { String(format: "0x%04X, which matches %@", $0, imageFileName) } ?? "not a CRC-16, so not checked"
        let requirements = initPacket.softDeviceRequirements.map { String(format: "0x%04X", $0) }.joined(separator: ", ")
        return """
            \(name)
              image type:      \(imageType)
              image:           \(imageFileName), \(image.count) bytes
              start packet:    \(ControlPointRequest.startDFU(imageType).bytes.hex), then sizes \(imageSizes.bytes.hex)
              init packet:     \(initPacketFileName), \(initPacket.bytes.count) bytes: \(initPacket.bytes.hex)
                device type \(String(format: "0x%04X", initPacket.deviceType)), revision \(String(format: "0x%04X", initPacket.deviceRevision)), \
            application version \(String(format: "0x%08X", initPacket.applicationVersion)), SoftDevices [\(requirements)]
                image CRC-16 \(crc)
              upload:          \(packets) packets of up to \(settings.packetSize) bytes, a receipt every \(interval) packets (\(receipts) receipts)
            """
    }
}

extension [UInt8] {
    var hex: String { map { String(format: "%02X", $0) }.joined(separator: " ") }
}
