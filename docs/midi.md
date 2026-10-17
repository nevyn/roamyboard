# MIDI

The central (the left half, or a unibody) is a Bluetooth LE MIDI device as well as a keyboard: it offers the BLE-MIDI GATT service beside ZMK's HID service, on the same connection to each host. A MIDI key sends its messages to the host on the active Bluetooth profile, just as a key press goes to that host.

## Keys

`zmk/dts/behaviors/midi.dtsi` defines the MIDI keys; `zmk/src/behaviors/behavior_midi.c` runs them on the central, over the pure state in `zmk/src/midi/state.c`, which `zmk/tests/midi_state_test.c` tests.

| Binding | Does |
|---|---|
| `&midi_note N` | Note N (60 is middle C, C4), shifted by the octave and transpose, on the current channel while held. |
| `&midi_drum N` | Note N as it is, on channel 10 (General MIDI drums), while held. |
| `&midi_ctl MIDI_…` | On press: `MIDI_OCT_DN`/`UP` step the octave (±3), `MIDI_TR_DN`/`UP` the transpose (±11 semitones), `MIDI_VEL_DN`/`UP` the velocity (pp 40, p 64, mf 90, f 110, ff 127; mf at start), `MIDI_CH_DN`/`UP` the channel (1 to 16). `MIDI_PANIC` releases every held note, then sends All Notes Off and sustain off on all 16 channels. `dt-bindings/roamyboard/midi.h` lists them. |
| `&midi_cc CC VAL` | Control change CC to VAL on press and to 0 on release, on the current channel. `&midi_cc 64 127` is a sustain pedal. |

A held key remembers the note and channel that it sent, so its note off matches its note on even when the octave, transpose or channel changes in between. The state starts over at every boot.

The split keymap has two MIDI layers, Piano and Grid, reached with To5 and To6 on the System layer; [Layout](../zmk/Layout.md) shows them.

## GATT service

`zmk/src/midi/ble_midi.c` defines the service from the BLE-MIDI specification:

| | UUID | Properties |
|---|---|---|
| Service | `03B80E5A-EDE8-4B33-A751-6CE34EC4C700` | |
| MIDI I/O characteristic | `7772E5DB-3868-4112-A1A9-F2669D106BF3` | read (always empty), write without response, notify; encryption required |

Each notification carries one message: a header byte (`0x80` | timestamp bits 12 to 7), a timestamp byte (`0x80` | timestamp bits 6 to 0), then the MIDI 1.0 message. The timestamp is the key event's uptime in milliseconds, modulo 8192, so the host can space the messages as they were played, whatever the connection interval. Messages that the host writes are ignored.

`roamyboard_midi_send()` (`zmk/include/roamyboard/midi.h`) queues a message; a work queue of its own sends the notifications, as ZMK's HID service does, because a notification on the system work queue cannot wait for a buffer. A message is dropped when the active profile has no connection or its host has not subscribed.

## Hosts

macOS and iPadOS bring the keyboard's MIDI online by themselves once it is paired as a keyboard, though its advertisement lists only HID (tried 2026-10-17): it shows up as the MIDI source "roamyboard Bluetooth", and in iPadOS's Bluetooth MIDI device list. A host that paired before the firmware had the service learns of it from the GATT Service Changed indication; if it does not, removing the keyboard from the host's Bluetooth settings and pairing again fixes that.

`roamy-midi` (`zmk/tools/midi`) checks the connection from a Mac, run from the user's own terminal app:

```sh
swift build -c release --package-path zmk/tools/midi
zmk/tools/midi/.build/release/roamy-midi --listen   # print MIDI only
zmk/tools/midi/.build/release/roamy-midi            # also hand the keyboard to CoreMIDI
```

It prints every message from every MIDI source, with the gap since the previous message. Without `--listen`, for a host that does not bring the keyboard online by itself, it first finds the keyboard among the connected peripherals, by the MIDI service or else by its name among HID peripherals, lists its services, and calls `MIDIBluetoothDriverActivateAllConnections()`, which turns a CoreBluetooth connection to a BLE-MIDI service into a CoreMIDI device.

## Known gaps

- Every gap between notes that the Mac received from the right half was a multiple of 15 ms, so the host may time the notes by their arrival rather than by their BLE-MIDI timestamps.
- Only the split keymap has the MIDI layers.
- Nothing shows the octave, transpose, velocity or channel.
