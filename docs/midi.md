# MIDI

The central (the left half, or a unibody) is a Bluetooth LE MIDI device as well as a keyboard: it offers the BLE-MIDI GATT service beside ZMK's HID service, on the same connection to each host. A MIDI key sends its messages to the host on the active Bluetooth profile, just as a key press goes to that host.

## Keys

`&midi_note N` (`zmk/dts/behaviors/midi.dtsi`) sends note on for MIDI note N, channel 1, velocity 100, when the key goes down, and note off when it comes up. 60 is middle C, C4. The System layer has C4 and E4 on the 8 and 9 keys.

## GATT service

`zmk/src/midi/ble_midi.c` defines the service from the BLE-MIDI specification:

| | UUID | Properties |
|---|---|---|
| Service | `03B80E5A-EDE8-4B33-A751-6CE34EC4C700` | |
| MIDI I/O characteristic | `7772E5DB-3868-4112-A1A9-F2669D106BF3` | read (always empty), write without response, notify; encryption required |

Each notification carries one message: a header byte (`0x80` | timestamp bits 12 to 7), a timestamp byte (`0x80` | timestamp bits 6 to 0), then the MIDI 1.0 message. The timestamp is the key event's uptime in milliseconds, modulo 8192, so the host can space the messages as they were played, whatever the connection interval. Messages that the host writes are ignored.

`roamyboard_midi_send()` (`zmk/include/roamyboard/midi.h`) queues a message; a work queue of its own sends the notifications, as ZMK's HID service does, because a notification on the system work queue cannot wait for a buffer. A message is dropped when the active profile has no connection or its host has not subscribed.

## Hosts

macOS 13 and later bring a paired BLE-MIDI peripheral online by themselves. If the keyboard was paired before its firmware had the service, macOS may keep the old GATT database; removing the keyboard in System Settings > Bluetooth and pairing again fixes that.

`roamy-midi` (`zmk/tools/midi`) checks the connection from a Mac, run from the user's own terminal app:

```sh
swift build -c release --package-path zmk/tools/midi
zmk/tools/midi/.build/release/roamy-midi --listen   # print MIDI only
zmk/tools/midi/.build/release/roamy-midi            # also hand the keyboard to CoreMIDI
```

It prints every message from every MIDI source, with the gap since the previous message. Without `--listen`, it first finds the keyboard among the connected peripherals, by the MIDI service or else by its name among HID peripherals, lists its services, and calls `MIDIBluetoothDriverActivateAllConnections()`, which turns a CoreBluetooth connection to a BLE-MIDI service into a CoreMIDI device.

## Known gaps

- The advertisement lists only HID, as ZMK builds it (`zmk_ble_ad` in ZMK's `app/src/ble.c`), so a host that finds BLE-MIDI devices by scanning, such as iOS's Bluetooth MIDI picker, may not see the keyboard.
- Channel and velocity are fixed; there is no MIDI layer yet.
