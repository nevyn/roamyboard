/*
 * The roamyboard status screen on the nice!view (zmk/src/display).
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/**
 * Replaces the status screen with the bootloader view, renders it, writes it to the display,
 * and then calls done. The screen shows nothing else afterwards; done is expected to reboot.
 *
 * Callable from any thread; returns without waiting.
 *
 * @param done Called once, on the display work queue, after the display has received the
 *             whole bootloader view. Not called if this function returns an error.
 * @return 0 if the bootloader view is on its way, or -ENODEV if the status screen has not
 *         been created yet (the display is not initialized).
 */
int roamyboard_status_screen_show_bootloader(void (*done)(void));
