/*
 * LVGL configuration for rendering the status screen on the host: the nice!view's
 * settings from the firmware's .config that matter for drawing.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 1
#define LV_USE_OS LV_OS_NONE
#define LV_MEM_SIZE (256 * 1024)
#define LV_DRAW_BUF_STRIDE_ALIGN 1
#define LV_DRAW_BUF_ALIGN 4
#define LV_USE_DRAW_SW 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_UNSCII_8 1
#define LV_FONT_DEFAULT &lv_font_unscii_8
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0
#define LV_USE_CANVAS 1
#define LV_USE_IMAGE 1
#define LV_USE_LABEL 1
#define LV_USE_LOG 1
#define LV_LOG_PRINTF 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_TXT_ENC LV_TXT_ENC_UTF8

#endif
