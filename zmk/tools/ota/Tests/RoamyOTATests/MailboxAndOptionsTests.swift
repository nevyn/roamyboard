import Testing
@testable import RoamyOTA

@Suite(.timeLimit(.minutes(1)))
struct MailboxTests {
    @Test func deliversQueuedValuesBeforeTheCloseError() async throws {
        let mailbox = Mailbox<Int>()
        mailbox.post(1)
        mailbox.post(2)
        mailbox.close(throwing: DFUTransportError.disconnected(nil))
        mailbox.post(3)

        #expect(try await mailbox.receive() == 1)
        #expect(try await mailbox.receive() == 2)
        await #expect(throws: DFUTransportError.disconnected(nil)) { try await mailbox.receive() }
    }

    @Test func closeResumesAWaitingReceiver() async throws {
        let mailbox = Mailbox<Int>()
        let receiver = Task { try await mailbox.receive() }
        try await Task.sleep(for: .milliseconds(20))
        mailbox.close(throwing: DFUTransportError.disconnected("gone"))
        await #expect(throws: DFUTransportError.disconnected("gone")) { try await receiver.value }
    }

    @Test func cancellationResumesAWaitingReceiver() async throws {
        let mailbox = Mailbox<Int>()
        let receiver = Task { try await mailbox.receive() }
        try await Task.sleep(for: .milliseconds(20))
        receiver.cancel()
        await #expect(throws: CancellationError.self) { try await receiver.value }

        mailbox.post(7)
        #expect(try await mailbox.receive() == 7)
    }

    @Test func timeoutCancelsAWaitingReceiverInsteadOfHanging() async throws {
        let mailbox = Mailbox<Int>()
        await #expect(throws: TimeoutError(duration: .milliseconds(50))) {
            try await withTimeout(.milliseconds(50)) { try await mailbox.receive() }
        }
    }
}

@Suite struct OptionsTests {
    @Test func defaults() throws {
        let options = try Options.parse(["left.zip", "right.zip"])
        #expect(options.inputs == ["left.zip", "right.zip"])
        #expect(options.packetReceiptInterval.packets == 8)
        #expect(options.packetSize == 20)
        #expect(options.discoveryTimeout == .seconds(120))
        #expect(!options.dryRun)
    }

    @Test func flags() throws {
        let options = try Options.parse(["--prn", "4", "--dry-run", "--timeout", "30", "left.zip"])
        #expect(options.packetReceiptInterval.packets == 4)
        #expect(options.dryRun)
        #expect(options.discoveryTimeout == .seconds(30))
    }

    @Test(arguments: ["0", "9", "16"])
    func packetReceiptIntervalOutsideOneToEightIsRefused(value: String) {
        let error = #expect(throws: OptionError.self) { try Options.parse(["--prn", value, "left.zip"]) }
        #expect(error?.description.contains("runs out of memory above 8") == true)
    }

    @Test func mistakes() {
        #expect(throws: OptionError.noInputs) { try Options.parse(["--prn", "8"]) }
        #expect(throws: OptionError.missingValue(option: "--prn")) { try Options.parse(["left.zip", "--prn"]) }
        #expect(throws: OptionError.unknownOption("--fast")) { try Options.parse(["--fast", "left.zip"]) }
        #expect(throws: OptionError.self) { try Options.parse(["--packet-size", "22", "left.zip"]) }
    }
}
