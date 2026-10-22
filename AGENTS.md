# roamyboard

@README.md
@docs/index.md

## Hardware test checkpoints

Whenever a change needs Nevyn to test it on the keyboard, build the firmware locally and hand over an over-the-air update, not UF2s:

1. Build each affected half with Docker as in [firmware.md](docs/firmware.md) (Building), with the west workspace in `~/.cache/roamyboard-west`, which survives between sessions (`/tmp` gets cleaned).
2. Pack a DFU zip per half with `adafruit-nrfutil` 0.5.3.post16 (venv in `~/.venvs/nrfutil`), with the same `dfu genpkg` arguments as `.github/workflows/build.yml`, into `fw/<build>/<build>.zip`.
3. Build `roamy-ota` (`swift build -c release --package-path zmk/tools/ota`) and run `roamy-ota --dry-run` on the zips.
4. Give Nevyn the `roamy-ota` command, naming only the halves whose firmware changed, and say which OTA key to press on each half. He runs it from his own terminal app: macOS kills a Bluetooth client in an agent's shell.

## Debugging on the keyboard

Reach for the keyboard's own logs before guessing or flashing a special build. Every half keeps its most recent log records in an 8 KB backlog, and `roamy-log` (`zmk/tools/log`, [logging.md](docs/logging.md)) reads both halves over Bluetooth from the Mac that the keyboard is connected to:

1. Build it: `swift build -c release --package-path zmk/tools/log`.
2. Give Nevyn the command and ask him to paste its output. He runs it from his own terminal app, as with `roamy-ota`. Plain `zmk/tools/log/.build/release/roamy-log` prints the backlog first, so it also shows what happened before he started it.
3. Add `--level zmk=dbg` for the debug logs of ZMK and the roamyboard code (every key event), `--half L` or `--half R` for one half, and `--save FILE` for a long capture.

The USB logging build in [firmware.md](docs/firmware.md) is the fallback when Bluetooth is down or the firmware predates the log service.
