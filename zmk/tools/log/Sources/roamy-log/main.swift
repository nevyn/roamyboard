import Foundation
import RoamyLog

let options: Options
do {
    options = try Options.parse(Array(CommandLine.arguments.dropFirst()))
} catch {
    FileHandle.standardError.write(Data("roamy-log: \(error)\n\n\(Options.usage)\n".utf8))
    exit(64)
}
if options.help {
    print(Options.usage)
    exit(0)
}

setvbuf(stdout, nil, _IOLBF, 0)
let client = MainActor.assumeIsolated { LogClient(options: options) }

// Ctrl-C ends the session first, so that the firmware sets its levels back to info; a second one quits at once.
var signalSources: [DispatchSourceSignal] = []
for number in [SIGINT, SIGTERM] {
    signal(number, SIG_IGN)
    let source = DispatchSource.makeSignalSource(signal: number, queue: .main)
    source.setEventHandler { MainActor.assumeIsolated { client.stop() } }
    source.resume()
    signalSources.append(source)
}

// dispatchMain() would drain the main queue on another thread, which @MainActor delegate callbacks reject.
RunLoop.main.run()
