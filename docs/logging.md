# Logging

Every roamyboard build logs over Bluetooth. Each half keeps its latest log records in RAM, the **backlog**, whether or not anyone listens. A Mac that the keyboard is paired with reads them with `roamy-log`: first the backlog, then every new record, from both halves of a split through the one connection to the left half. Debug logs are compiled in but off; `roamy-log` turns them on per log source while it runs. Nothing needs a special build or a USB cable; the USB logging build (`-S zmk-usb-logging`, [Firmware](firmware.md), Building) still works beside it.

## roamy-log

`zmk/tools/log` is a Swift package that builds `roamy-log`. Build it once, and again whenever `zmk/tools/log` changes:

```sh
swift build -c release --package-path zmk/tools/log
```

Run it from a terminal app. On the first run, macOS asks whether the terminal app may use Bluetooth; a process that macOS cannot attribute to an app that may ask, such as an agent's shell, is killed instead.

```sh
zmk/tools/log/.build/release/roamy-log                      # the backlog, then follow
zmk/tools/log/.build/release/roamy-log --level zmk=dbg      # with ZMK's debug logs
zmk/tools/log/.build/release/roamy-log --no-backlog --half R --save ~/roamy.log
zmk/tools/log/.build/release/roamy-log --list-modules       # every log source and its level
```

Each record is one line: the half (`L`, `R`, or `U` for a unibody), the half's uptime, the level (`err`, `wrn`, `inf`, `dbg`, or `raw` for printk), the log source and the text. A text with line breaks goes on over indented lines, and a text that the firmware cut to 192 bytes ends in `[cut]`.

```
L 00:00:00.412 inf zmk: Welcome to ZMK!
R 00:00:01.120 inf roamy_log: log session began at record 0, with 37 from the backlog
L -- end of backlog; live from here --
L -- 3 records dropped --
R -- restarted --
```

Lines that start with `--` are events: the end of a half's backlog, records that never arrived, and a half that started again. Records and events go to stdout; `roamy-log`'s own messages, such as "lost the keyboard; waiting for it to come back", go to stderr.

| Option | Does |
| --- | --- |
| `--level MODULE=LEVEL` | Sets the level of one log source, or of every source with `all`, on every half: `off`, `err`, `wrn`, `inf` or `dbg`. Repeatable, applied in order. |
| `--half L\|R\|U` | Shows only that half's records. Level commands still go to every half. |
| `--save FILE` | Also appends every line to FILE. |
| `--no-backlog` | Shows only records from now on. |
| `--list-modules` | Prints every half's log sources with their levels, then quits. |
| `--name NAME` | The keyboard's Bluetooth name, for when no connected peripheral offers the log service (default `roamyboard`). |
| `--timeout SECONDS` | How long to look for the keyboard (default 15). |

`roamy-log` asks macOS for the peripherals that are connected to the Mac and offer the log service (`retrieveConnectedPeripherals(withServices:)`); Apple documents that this includes peripherals that other apps connected, which should cover the keyboard that the system holds as a HID keyboard (Known gaps, below). If none offers it, it looks for a connected HID peripheral named `--name`, and then scans for one that advertises. It connects, subscribes to the records, starts a log session, and sends the level commands.

When the link drops, `roamy-log` waits for the keyboard to come back, starts a new session where each half's records left off (`start L:n R:m`, below), and sets the levels again. Ctrl-C ends the session first, so that the firmware sets its levels back to info; a second Ctrl-C quits at once.

`roamy-log` names the cause when it cannot go on: no keyboard connected or advertising; a keyboard without the log service, whose firmware is older or whose GATT database macOS still has from before the update (remove the keyboard in System Settings > Bluetooth and pair it again); a link that is not encrypted because the keyboard is not paired with this Mac; a command that the firmware does not understand; and a frame format version that it does not know.

## Backlog, sessions and levels

`zmk/src/log/backend.c` is a Zephyr log backend in every build (`CONFIG_ROAMYBOARD_LOG`, on by default). It renders each log message that passes its filter as text and appends a record to the half's backlog: a ring of `CONFIG_ROAMYBOARD_LOG_BACKLOG_SIZE` bytes (8192) in RAM, which drops its oldest records to make room (`zmk/src/log/ring.c`). Every record gets the next sequence number of its half, from 0 at the half's start. A record takes its text, its source name and 8 bytes, so the backlog holds the latest 100 to 200 records.

A **log session** streams one half's records to one host: first the records from where the session begins, then each new one. The host subscribes to the records characteristic and writes a start command; the session ends when the host writes `stop`, unsubscribes or disconnects. A new start command from any host begins a new session, which replaces the old one.

Zephyr compiles every source's log calls up to its compiled level: ZMK's code and roamyboard's (source `zmk`) up to debug, since ZMK sets `CONFIG_ZMK_LOG_LEVEL` to 4 by default, and Zephyr's subsystems up to info (`CONFIG_LOG_DEFAULT_LEVEL`). On top of that, each backend has a runtime filter per source (`CONFIG_LOG_RUNTIME_FILTERING`). The log core enables a backend at every source's compiled level; this backend sets its own filter to info for every source when it processes its first message, and drops messages above the filter until then. A level command moves the filter of one source or of all; the filter cannot rise above a source's compiled level, and the answer says when it did not. When a session ends, every source goes back to info.

Almost all of ZMK's and roamyboard's code logs as one source, `zmk`, so `zmk=dbg` turns on its debug logs everywhere, including every key event. The Bluetooth stack's sources (`bt_hci_core`, `bt_conn` and so on) are compiled up to info, so they cannot go to debug without a build that raises their own `CONFIG_BT_*_LOG_LEVEL`. `--list-modules` shows what a build has.

The filters are per backend, so the USB logging build's backend keeps its own levels: it is enabled at every source's compiled level as before, and the aggregate filter that decides whether a log call makes a message at all is the highest of the two.

The log service writes records about itself, from source `roamy_log`: when a session begins or ends, the answer to each level command, the source list, problems with the relay, and messages that the log core dropped before they reached the backend. These go straight into the backlog, past the filters and the other backends.

## Both halves through the left half

The right half has no host; its only connection is the split link to the left half. It has the same log backend and log service as every half, and the left half is the host of its sessions: `zmk/src/log/relay.c` on the split central discovers the right half's log service once the split link is encrypted, as the PDA does ([Micro PDA](pda.md), Split data channel), asks for a larger ATT MTU (65 bytes instead of 23), and subscribes. The split data channel was not a fit: its messages carry at most 20 bytes, and it exists only with the PDA.

When a host begins a session on the left half, the left half writes the same start command to the right half, which begins its own session with the left half. The left half puts each of the right half's records back together from its fragments, keeps it in a relay ring of `CONFIG_ROAMYBOARD_LOG_RELAY_SIZE` bytes (2048), and sends it to the host before its own, cut to the host's MTU. The right half's records keep their sequence numbers and half, so the host tells the halves apart and sees the right half's gaps. Level and list commands go to both halves, and each half answers for itself; a source that only one half has gets "no log source" from the other.

When the split link drops during a session, the right half's session ends and its levels go back to info. When it comes back, the left half starts the right half's session again from the record after the last one that it received (`start R:n`), and sends the session's latest level commands again. When the host's session ends, the left half writes `stop` to the right half.

## GATT service

`zmk/src/log/service.c` defines the service; `zmk/src/log/internal.h` holds the UUIDs.

| | UUID | Properties | Permissions |
| --- | --- | --- | --- |
| Service | `7BEBA2B0-68D2-4D86-A34C-647827C1B55B` | | |
| Records | `7BEBA2B1-68D2-4D86-A34C-647827C1B55B` | notify | read encrypted (no reads); CCC read and write encrypted |
| Command | `7BEBA2B2-68D2-4D86-A34C-647827C1B55B` | write, write without response | write encrypted |

Every characteristic and the CCC need an encrypted link, which only a bonded host has. A write of a command that does not parse gets ATT error `0x80`; one that comes while four commands wait gets "insufficient resources". Commands are ASCII, one per write, at most 64 bytes:

| Command | Does |
| --- | --- |
| `start` | Begins a session from the oldest record in the backlog. |
| `start U:n L:n R:n` | Begins a session; each half named sends from its record n, the others from their oldest. A record that the backlog has dropped, or an n beyond the half's next record (from before a restart), starts from the oldest. |
| `live` | Begins a session from the next record on. |
| `stop` | Ends the session; every level goes back to info. |
| `MODULE=LEVEL` | Sets the level of the log source MODULE (1 to 32 letters, digits, `_`, `.` or `-`), or of every source with `all`: `off`, `err`, `wrn`, `inf` or `dbg`. |
| `list` | Writes the half's sources and levels as records: `sources 1-12 of 40: zmk=inf bt_hci_core=inf ...`. |

The session's host must subscribe before it starts the session: the half sends records only while the session's host is subscribed.

## Frame format

`zmk/src/log/frame.h` writes it and `zmk/tools/log/Sources/RoamyLog/Frame.swift` reads it. `zmk/tests/log_test.c` writes `zmk/tools/log/Tests/RoamyLogTests/Fixtures/log_frames.txt` with the firmware's encoder, `swift test` decodes it, and `zmk/tests/run.sh` fails when the encoder no longer writes the committed file (`zmk/tests/run.sh --write-log-fixture` rewrites it). All numbers are little-endian.

Each notification is one **fragment** of one record: a 6-byte header, then the next bytes of the record's body, as many as the notification has room for (the ATT MTU less 3, at least 20).

| Byte | Field |
| --- | --- |
| 0 | Flags: bits 0 and 1 the half (0 unibody, 1 left, 2 right), bit 2 the last fragment of the record, bit 3 recorded before the session began, bits 4 to 7 the format version, 1 |
| 1 | Fragment index, 0 for a record's first |
| 2 to 5 | The record's sequence number (uint32), which counts the half's records from 0 at its start |

A half sends a record's fragments one after the other, in index order, before the next record's. The body:

| Byte | Field |
| --- | --- |
| 0 to 3 | The half's uptime in milliseconds when the message was logged (uint32; wraps after 49.7 days) |
| 4 | Bits 0 to 2 the level: 0 printk, 1 error, 2 warning, 3 info, 4 debug. Bit 7: the text was cut to 192 bytes. |
| 5 | The source name's length, 0 to 32 |
| 6 | The source name, UTF-8; empty for printk |
| 6 + length | The text, UTF-8, to the end of the body; at most 192 bytes, cut at a character boundary. A hexdump's bytes follow the text in hex. |

A body is at most 230 bytes. A reader drops a fragment that does not continue the record in progress, and the rest of that record. Within a half, sequence numbers rise by one per record; a gap means that the half dropped records before it could send them (the backlog, or on the left half the relay ring), and a sequence number or uptime that goes down means that the half started again.

## Security

The log service and its CCC need an encrypted link, so only a host that is bonded with the keyboard can subscribe or send commands; on the split, the halves' link is bonded. At debug level the `zmk` source logs every key event, so a session at `zmk=dbg` carries everything that is typed, over that encrypted link. Levels go back to info when the session ends.

## Costs

Measured on builds of this branch against origin/main (2a662eb), from the linker's memory report:

| Build | Flash, main | Flash, with logging | RAM, main | RAM, with logging |
| --- | --- | --- | --- | --- |
| `roamyboard` | 432,204 B | 503,608 B (+71,404) | 116,346 B | 131,626 B (+15,280) |
| `roamyboard_left` | 472,664 B | 557,660 B (+84,996) | 128,774 B | 147,374 B (+18,600) |
| `roamyboard_right` | 448,736 B | 497,612 B (+48,876) | 101,448 B | 116,672 B (+15,224) |
| `roamyboard_left` with `zmk-usb-logging` | 542,852 B | 558,332 B (+15,480) | 140,774 B | 156,006 B (+15,232) |

Flash: Zephyr's logging core and the log strings of every source cost most of it, which the USB logging build already paid; the debug strings of the `zmk` source cost 21.7 KB on the left half (a build with `CONFIG_ZMK_LOGGING_MINIMAL` is that much smaller), and the log service's own code about 7 KB. All builds stay below the 792 KB application area. RAM: the backlog (8 KB), the relay ring on the left half (2 KB), the log core's message buffer (`CONFIG_LOG_BUFFER_SIZE`, 2 KB, 8 KB with USB logging as before), and the stacks of the log thread (1 KB) and the log service's work queue (1.5 KB).

CPU, not measured on hardware: every log call checks the runtime filter, a few instructions, even when its level is off. The log thread renders each message that passes the filter as text, at the lowest application priority, so that key scanning and key reports go first. It wakes 100 ms after the first message (`CONFIG_LOG_PROCESS_THREAD_SLEEP_MS`) or after ten, and not while nothing is logged. A session sends at most two notifications at a time, so the log service leaves the other ATT buffers to the key reports. With `zmk=dbg` the keyboard logs several messages per key event; whether that delays keys or drains the battery noticeably is not known yet.

## Known gaps

- Nobody has run it on the keyboard yet: the firmware builds and the host tests pass, but no Mac has read a log session from it.
- Apple's documentation says that `retrieveConnectedPeripherals(withServices:)` includes peripherals that other apps, and so the system, have connected; ZMK Studio uses the same kind of encrypted custom GATT service beside HID. That macOS lets `roamy-log` open the log service on the keyboard that it holds as a HID keyboard has not been tried.
- Whether macOS writes the records CCC back to off when `roamy-log` is killed without its Ctrl-C handler is not known. If it does not, the session goes on until the keyboard disconnects, with its levels.
- Uptime wraps after 49.7 days, which `roamy-log` reports as a restart.
- With `--no-backlog`, a reconnect before a half has sent any record gets that half's whole backlog.
- One host at a time has a session; a second host's start command takes the session over.
- The log core's messages are rendered on its thread up to 100 ms after they were logged; the uptime in a record is the time of the log call, not of the rendering.
