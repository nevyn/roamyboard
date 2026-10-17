/*
 * MIDI to the host over Bluetooth LE MIDI, on the connection of the active Bluetooth profile.
 * docs/midi.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

/** The longest MIDI message that roamyboard_midi_send() takes: a channel voice message. */
#define ROAMYBOARD_MIDI_MSG_MAX 3

/**
 * Queues one MIDI 1.0 message for the host on the active Bluetooth profile. The BLE-MIDI
 * timestamp is the low 13 bits of timestamp_ms, so the host can space the messages as they
 * were played, whatever the connection interval.
 *
 * The message goes out from a work queue of its own, and is dropped there, with a debug log,
 * if the active profile is not connected or its host has not subscribed to MIDI notifications.
 *
 * @param msg A status byte and its data bytes, for example {0x90, 60, 100} for note on,
 *            channel 1, middle C, velocity 100. No running status, no SysEx.
 * @param len 1 to ROAMYBOARD_MIDI_MSG_MAX.
 * @param timestamp_ms Uptime in milliseconds when the message was played, such as a
 *                     behavior event's timestamp.
 * @return 0 when queued, -EINVAL for a bad length or a missing status byte, -ENOMEM when
 *         the queue stayed full for 100 ms.
 */
int roamyboard_midi_send(const uint8_t *msg, size_t len, int64_t timestamp_ms);
