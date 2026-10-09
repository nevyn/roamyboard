import Foundation

/// GATT identifiers of Nordic's legacy DFU service (nRF5 SDK 11), which the Adafruit nRF52
/// bootloader serves in OTA mode.
public enum LegacyDFUService {
    /// The DFU service. The bootloader also advertises it, next to the name "AdaDFU".
    public static let service = "00001530-1212-EFDE-1523-785FEABCD123"
    /// Takes requests (write with response) and sends responses and packet receipts (notify).
    public static let controlPoint = "00001531-1212-EFDE-1523-785FEABCD123"
    /// Takes the image sizes, the init packet and the image (write without response).
    public static let packet = "00001532-1212-EFDE-1523-785FEABCD123"
    /// The DFU revision, two bytes, minor first (read).
    public static let revision = "00001534-1212-EFDE-1523-785FEABCD123"
}

/// The first byte of every control point request and notification.
public enum OpCode: UInt8, Sendable, CustomStringConvertible {
    case startDFU = 1
    case initDFUParameters = 2
    case receiveFirmwareImage = 3
    case validateFirmware = 4
    case activateAndReset = 5
    case reset = 6
    case reportReceivedImageSize = 7
    case packetReceiptNotificationRequest = 8
    case response = 16
    case packetReceiptNotification = 17

    public var description: String {
        switch self {
        case .startDFU: "Start DFU"
        case .initDFUParameters: "Initialize DFU Parameters"
        case .receiveFirmwareImage: "Receive Firmware Image"
        case .validateFirmware: "Validate Firmware"
        case .activateAndReset: "Activate and Reset"
        case .reset: "Reset"
        case .reportReceivedImageSize: "Report Received Image Size"
        case .packetReceiptNotificationRequest: "Packet Receipt Notification Request"
        case .response: "Response"
        case .packetReceiptNotification: "Packet Receipt Notification"
        }
    }
}

/// What a DFU transfer replaces, as a bit in the Start DFU request.
public enum ImageType: UInt8, Sendable, CustomStringConvertible {
    case softDevice = 1
    case bootloader = 2
    case application = 4

    public var description: String {
        switch self {
        case .softDevice: "SoftDevice"
        case .bootloader: "bootloader"
        case .application: "application"
        }
    }
}

/// A request written to the control point, with response.
public enum ControlPointRequest: Equatable, Sendable, CustomStringConvertible {
    /// Starts a transfer of `ImageType`. The bootloader answers after it has also received the
    /// `ImageSizes` on the packet characteristic and erased the old image.
    case startDFU(ImageType)
    /// Announces the init packet, which follows on the packet characteristic. No response.
    case initPacketBegin
    /// Ends the init packet. The bootloader answers once it has checked the init packet.
    case initPacketComplete
    /// Asks for a packet receipt after every `n` image packets; 0 turns receipts off. No response.
    case packetReceiptNotificationRequest(UInt16)
    /// Announces the image on the packet characteristic. The bootloader answers once it has
    /// written the last byte to flash.
    case receiveFirmwareImage
    /// Asks the bootloader to check the received image against the init packet's CRC.
    case validateFirmware
    /// Asks the bootloader to boot the new image. It disconnects instead of answering.
    case activateAndReset
    /// Abandons the transfer; the bootloader restarts.
    case reset

    /// The bytes written to the control point.
    public var bytes: [UInt8] {
        switch self {
        case .startDFU(let type): [OpCode.startDFU.rawValue, type.rawValue]
        case .initPacketBegin: [OpCode.initDFUParameters.rawValue, 0]
        case .initPacketComplete: [OpCode.initDFUParameters.rawValue, 1]
        case .packetReceiptNotificationRequest(let n):
            [OpCode.packetReceiptNotificationRequest.rawValue, UInt8(n & 0xFF), UInt8(n >> 8)]
        case .receiveFirmwareImage: [OpCode.receiveFirmwareImage.rawValue]
        case .validateFirmware: [OpCode.validateFirmware.rawValue]
        case .activateAndReset: [OpCode.activateAndReset.rawValue]
        case .reset: [OpCode.reset.rawValue]
        }
    }

    /// Parses the bytes of a control point write; nil if they are not a request this tool sends.
    public init?(bytes: [UInt8]) {
        guard let first = bytes.first, let op = OpCode(rawValue: first) else { return nil }
        switch (op, bytes.count) {
        case (.startDFU, 2):
            guard let type = ImageType(rawValue: bytes[1]) else { return nil }
            self = .startDFU(type)
        case (.initDFUParameters, 2) where bytes[1] == 0: self = .initPacketBegin
        case (.initDFUParameters, 2) where bytes[1] == 1: self = .initPacketComplete
        case (.packetReceiptNotificationRequest, 3):
            self = .packetReceiptNotificationRequest(UInt16(bytes[1]) | UInt16(bytes[2]) << 8)
        case (.receiveFirmwareImage, 1): self = .receiveFirmwareImage
        case (.validateFirmware, 1): self = .validateFirmware
        case (.activateAndReset, 1): self = .activateAndReset
        case (.reset, 1): self = .reset
        default: return nil
        }
    }

    /// The op code that the bootloader names in its response to this request.
    public var opCode: OpCode { OpCode(rawValue: bytes[0])! }

    public var description: String {
        switch self {
        case .startDFU(let type): "Start DFU (\(type))"
        case .initPacketBegin: "Initialize DFU Parameters (begin)"
        case .initPacketComplete: "Initialize DFU Parameters (complete)"
        case .packetReceiptNotificationRequest(let n): "Packet Receipt Notification Request (\(n))"
        default: opCode.description
        }
    }
}

/// The status byte of a response. Unknown values are kept, so that a failure can name them.
public struct ResponseCode: RawRepresentable, Hashable, Sendable, CustomStringConvertible {
    public let rawValue: UInt8
    public init(rawValue: UInt8) { self.rawValue = rawValue }

    public static let success = ResponseCode(rawValue: 1)
    public static let invalidState = ResponseCode(rawValue: 2)
    public static let notSupported = ResponseCode(rawValue: 3)
    public static let dataSizeExceedsLimit = ResponseCode(rawValue: 4)
    public static let crcError = ResponseCode(rawValue: 5)
    public static let operationFailed = ResponseCode(rawValue: 6)

    /// The code and Nordic's name for it, for example "5 (CRC error)".
    public var description: String {
        let name = switch self {
        case .success: "success"
        case .invalidState: "invalid state"
        case .notSupported: "not supported"
        case .dataSizeExceedsLimit: "data size exceeds limit"
        case .crcError: "CRC error"
        case .operationFailed: "operation failed"
        default: "unknown"
        }
        return "\(rawValue) (\(name))"
    }
}

/// A notification from the control point.
public enum ControlPointNotification: Equatable, Sendable {
    /// The bootloader's answer to the request with `request`'s op code.
    case response(request: OpCode, status: ResponseCode)
    /// The number of image bytes that the bootloader has taken so far, sent after every n image
    /// packets as set by `ControlPointRequest.packetReceiptNotificationRequest`.
    case packetReceipt(bytesReceived: UInt32)

    /// Parses a notification; nil if the bytes are neither a response nor a packet receipt.
    public init?(bytes: [UInt8]) {
        guard let first = bytes.first, let op = OpCode(rawValue: first) else { return nil }
        switch op {
        case .response where bytes.count >= 3:
            guard let request = OpCode(rawValue: bytes[1]) else { return nil }
            self = .response(request: request, status: ResponseCode(rawValue: bytes[2]))
        case .packetReceiptNotification where bytes.count >= 5:
            self = .packetReceipt(bytesReceived: UInt32(littleEndianBytes: bytes[1..<5]))
        default:
            return nil
        }
    }

    /// The bytes that the bootloader notifies.
    public var bytes: [UInt8] {
        switch self {
        case .response(let request, let status): [OpCode.response.rawValue, request.rawValue, status.rawValue]
        case .packetReceipt(let n): [OpCode.packetReceiptNotification.rawValue] + n.littleEndianBytes
        }
    }
}

/// The sizes that follow Start DFU on the packet characteristic: three little-endian UInt32s.
public struct ImageSizes: Equatable, Sendable {
    public var softDevice: UInt32 = 0
    public var bootloader: UInt32 = 0
    public var application: UInt32 = 0

    public init(softDevice: UInt32 = 0, bootloader: UInt32 = 0, application: UInt32 = 0) {
        self.softDevice = softDevice
        self.bootloader = bootloader
        self.application = application
    }

    /// The 12 bytes written to the packet characteristic.
    public var bytes: [UInt8] {
        softDevice.littleEndianBytes + bootloader.littleEndianBytes + application.littleEndianBytes
    }

    /// Parses the 12-byte packet; nil for any other length.
    public init?(bytes: [UInt8]) {
        guard bytes.count == 12 else { return nil }
        softDevice = UInt32(littleEndianBytes: bytes[0..<4])
        bootloader = UInt32(littleEndianBytes: bytes[4..<8])
        application = UInt32(littleEndianBytes: bytes[8..<12])
    }
}

/// The value of the DFU revision characteristic.
public struct DFURevision: Equatable, Sendable, CustomStringConvertible {
    public var major: UInt8
    public var minor: UInt8

    public init(major: UInt8, minor: UInt8) {
        self.major = major
        self.minor = minor
    }

    /// Parses the characteristic's two bytes, minor first; nil for any other length.
    public init?(bytes: [UInt8]) {
        guard bytes.count == 2 else { return nil }
        minor = bytes[0]
        major = bytes[1]
    }

    /// True for 0.1, which Nordic's buttonless application-mode DFU service reports; a
    /// bootloader reports 0.5 or later.
    public var isApplicationMode: Bool { major == 0 && minor == 1 }

    public var description: String { "\(major).\(minor)" }
}

extension UInt32 {
    init(littleEndianBytes bytes: ArraySlice<UInt8>) {
        self = bytes.reversed().reduce(0) { $0 << 8 | UInt32($1) }
    }

    var littleEndianBytes: [UInt8] {
        [UInt8(self & 0xFF), UInt8(self >> 8 & 0xFF), UInt8(self >> 16 & 0xFF), UInt8(self >> 24)]
    }
}

extension UInt16 {
    init(littleEndianBytes bytes: ArraySlice<UInt8>) {
        self = UInt16(bytes[bytes.startIndex]) | UInt16(bytes[bytes.startIndex + 1]) << 8
    }

    var littleEndianBytes: [UInt8] { [UInt8(self & 0xFF), UInt8(self >> 8)] }
}
