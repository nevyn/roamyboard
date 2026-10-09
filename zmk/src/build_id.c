/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/init.h>
#include <zephyr/logging/log.h>

#include <roamyboard/build_id.h>
#include <roamyboard_build_id.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

const char *roamyboard_build_id(void) { return ROAMYBOARD_BUILD_ID; }

static int log_build_id(void) {
    LOG_INF("roamyboard %s", ROAMYBOARD_BUILD_ID);
    return 0;
}

SYS_INIT(log_build_id, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
