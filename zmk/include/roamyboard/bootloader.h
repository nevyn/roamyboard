/*
 * Rebooting into the nice!nano's Adafruit nRF52 bootloader, with its view on the status screen.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <roamyboard/status_screen.h>

/**
 * Shows the view for mode on the status screen, waits until the display has received it (at
 * most 1 s), and then reboots into that mode of the bootloader. Without the status screen, or
 * before it is created, it reboots at once. &boot_screen, &ota_boot and the split peripheral's
 * local OTA key call it; docs/firmware.md, "Choosing the bootloader mode", describes how each
 * mode is selected.
 *
 * Callable from any thread. Returns at once while the view is on its way; the reboot follows
 * on the display work queue. If the reboot cannot select the mode, it logs an error and the
 * firmware keeps running.
 *
 * @param mode The bootloader mode to reboot into.
 */
void roamyboard_enter_bootloader(enum roamyboard_bootloader_mode mode);
