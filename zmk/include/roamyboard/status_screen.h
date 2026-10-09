/*
 * The roamyboard status screen on the nice!view (zmk/src/display).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/** The modes of the Adafruit nRF52 bootloader that the bootloader keys reboot into. */
enum roamyboard_bootloader_mode {
    /** USB mass storage: the nice!nano mounts as NICENANO and takes a UF2 file. */
    ROAMYBOARD_BOOTLOADER_UF2,
    /** Bluetooth DFU: the bootloader advertises and takes a DFU zip from a phone. */
    ROAMYBOARD_BOOTLOADER_OTA,
};

/**
 * Replaces the status screen with the view for a bootloader mode, renders it, writes it to
 * the display, and then calls done. The screen shows nothing else afterwards; done is
 * expected to reboot.
 *
 * Callable from any thread; returns without waiting.
 *
 * @param mode Selects the view: the UF2 view or the OTA view.
 * @param done Called once, on the display work queue, after the display has received the
 *             whole view. Not called if this function returns an error.
 * @return 0 if the view is on its way, or -ENODEV if the status screen has not been created
 *         yet (the display is not initialized).
 */
int roamyboard_status_screen_show_bootloader(enum roamyboard_bootloader_mode mode,
                                             void (*done)(void));
