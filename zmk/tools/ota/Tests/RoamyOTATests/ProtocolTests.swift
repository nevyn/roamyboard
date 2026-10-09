import Testing
@testable import RoamyOTA

@Suite struct ProtocolTests {
    @Test func requestBytes() {
        #expect(ControlPointRequest.startDFU(.application).bytes == [0x01, 0x04])
        #expect(ControlPointRequest.initPacketBegin.bytes == [0x02, 0x00])
        #expect(ControlPointRequest.initPacketComplete.bytes == [0x02, 0x01])
        #expect(ControlPointRequest.packetReceiptNotificationRequest(8).bytes == [0x08, 0x08, 0x00])
        #expect(ControlPointRequest.packetReceiptNotificationRequest(0x0102).bytes == [0x08, 0x02, 0x01])
        #expect(ControlPointRequest.receiveFirmwareImage.bytes == [0x03])
        #expect(ControlPointRequest.validateFirmware.bytes == [0x04])
        #expect(ControlPointRequest.activateAndReset.bytes == [0x05])
        #expect(ControlPointRequest.reset.bytes == [0x06])
    }

    @Test(arguments: [
        ControlPointRequest.startDFU(.application), .initPacketBegin, .initPacketComplete,
        .packetReceiptNotificationRequest(8), .receiveFirmwareImage, .validateFirmware, .activateAndReset, .reset,
    ])
    func requestsRoundTrip(request: ControlPointRequest) {
        #expect(ControlPointRequest(bytes: request.bytes) == request)
    }

    @Test func notifications() {
        #expect(ControlPointNotification(bytes: [0x10, 0x01, 0x01]) == .response(request: .startDFU, status: .success))
        #expect(ControlPointNotification(bytes: [0x10, 0x04, 0x05]) == .response(request: .validateFirmware, status: .crcError))
        #expect(ControlPointNotification(bytes: [0x10, 0x03, 0x2A]) == .response(request: .receiveFirmwareImage, status: ResponseCode(rawValue: 42)))
        #expect(ControlPointNotification(bytes: [0x11, 0xA0, 0x86, 0x01, 0x00]) == .packetReceipt(bytesReceived: 100_000))
        #expect(ControlPointNotification.packetReceipt(bytesReceived: 100_000).bytes == [0x11, 0xA0, 0x86, 0x01, 0x00])
        #expect(ControlPointNotification(bytes: [0x11, 0x01]) == nil)
        #expect(ControlPointNotification(bytes: [0x10, 0x01]) == nil)
        #expect(ControlPointNotification(bytes: [0x42]) == nil)
    }

    @Test func responseCodesNameThemselves() {
        #expect(ResponseCode.operationFailed.description == "6 (operation failed)")
        #expect(ResponseCode(rawValue: 42).description == "42 (unknown)")
    }

    @Test func imageSizes() {
        let sizes = ImageSizes(application: 450_000)
        #expect(sizes.bytes == [0, 0, 0, 0, 0, 0, 0, 0, 0xD0, 0xDD, 0x06, 0x00])
        #expect(ImageSizes(bytes: sizes.bytes) == sizes)
        #expect(ImageSizes(bytes: [0, 0]) == nil)
    }

    @Test func revision() {
        let adafruit = DFURevision(bytes: [0x08, 0x00])
        #expect(adafruit == DFURevision(major: 0, minor: 8))
        #expect(adafruit?.isApplicationMode == false)
        #expect(DFURevision(bytes: [0x01, 0x00])?.isApplicationMode == true)
        #expect(DFURevision(bytes: [0x08]) == nil)
    }
}
