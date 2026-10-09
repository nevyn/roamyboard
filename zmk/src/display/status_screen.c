/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include <roamyboard/chain_state.h>
#include <roamyboard/events/chain_state_changed.h>
#include <roamyboard/status_screen.h>

#include "cat.h"
#include "screen.h"

#if IS_ENABLED(CONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180)
// For lv_display_t's flush_cb, which LVGL has no getter for.
#include <display/lv_display_private.h>
#endif

#if STATUS_SCREEN_CENTRAL
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#if IS_ENABLED(CONFIG_ROAMYBOARD_HOST_NAMES)
#include <roamyboard/events/host_name_changed.h>
#include <roamyboard/host_name.h>
#endif
#else
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#endif

#if IS_ENABLED(CONFIG_NICE_VIEW_WIDGET_STATUS)
#error "Set CONFIG_NICE_VIEW_WIDGET_STATUS=n: the roamyboard status screen replaces the nice!view's"
#endif

/** Shortest time between two walk frames; key presses in between are shown together. */
#define CAT_FRAME_MS 40

// Everything below is touched on the display work queue only, except the atomics.
static struct screen screen;
static struct status_state state;
static bool bootloader_shown;
static atomic_t ready;

static void redraw_top(void) {
    if (!bootloader_shown) {
        screen_draw_top(&screen, &state);
    }
}

static void set_battery_status(struct battery_status_state battery) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    state.charging = battery.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    state.battery = battery.level;

    redraw_top();
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = eh ? as_zmk_battery_state_changed(eh) : NULL;

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            set_battery_status, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

struct chain_status_state {
    int key_module_count;
};

static void set_chain_status(struct chain_status_state chain) {
    state.key_module_count = chain.key_module_count;
    redraw_top();
}

static struct chain_status_state chain_status_get_state(const zmk_event_t *eh) {
    // The driver updates the getter before it raises the event.
    return (struct chain_status_state){.key_module_count = roamyboard_chain_key_module_count()};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_chain_status, struct chain_status_state, set_chain_status,
                            chain_status_get_state)
ZMK_SUBSCRIPTION(widget_chain_status, roamyboard_chain_state_changed);

#if STATUS_SCREEN_CENTRAL

struct output_status_state {
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    bool profiles_connected[NICEVIEW_PROFILE_COUNT];
    bool profiles_bonded[NICEVIEW_PROFILE_COUNT];
};

struct layer_status_state {
    zmk_keymap_layer_index_t index;
    const char *label;
};

static void set_output_status(struct output_status_state output) {
    state.selected_endpoint = output.selected_endpoint;
    state.active_profile_index = output.active_profile_index;
    state.active_profile_connected = output.active_profile_connected;
    state.active_profile_bonded = output.active_profile_bonded;
    for (int i = 0; i < NICEVIEW_PROFILE_COUNT; ++i) {
        state.profiles_connected[i] = output.profiles_connected[i];
        state.profiles_bonded[i] = output.profiles_bonded[i];
    }

    if (!bootloader_shown) {
        screen_draw_top(&screen, &state);
        screen_draw_middle(&screen, &state);
    }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    struct output_status_state output = {
        .selected_endpoint = zmk_endpoint_get_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
    for (int i = 0; i < MIN(NICEVIEW_PROFILE_COUNT, ZMK_BLE_PROFILE_COUNT); ++i) {
        output.profiles_connected[i] = zmk_ble_profile_is_connected(i);
        output.profiles_bonded[i] = !zmk_ble_profile_is_open(i);
    }
    return output;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state, set_output_status,
                            output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

#if IS_ENABLED(CONFIG_ROAMYBOARD_HOST_NAMES)

static void set_host_name_status(struct roamyboard_host_name host) {
    state.active_host = host;

    if (!bootloader_shown) {
        screen_draw_middle(&screen, &state);
    }
}

static struct roamyboard_host_name host_name_status_get_state(const zmk_event_t *eh) {
    struct roamyboard_host_name host;
    roamyboard_host_name_get(zmk_ble_active_profile_index(), &host);
    return host;
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_host_name_status, struct roamyboard_host_name,
                            set_host_name_status, host_name_status_get_state)
ZMK_SUBSCRIPTION(widget_host_name_status, roamyboard_host_name_changed);
ZMK_SUBSCRIPTION(widget_host_name_status, zmk_ble_active_profile_changed);

#endif // IS_ENABLED(CONFIG_ROAMYBOARD_HOST_NAMES)

static void set_layer_status(struct layer_status_state layer) {
    state.layer_index = layer.index;
    state.layer_label = layer.label;

    if (!bootloader_shown) {
        screen_draw_bottom(&screen, &state);
    }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    zmk_keymap_layer_index_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){
        .index = index, .label = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(index))};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, set_layer_status,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

#else // STATUS_SCREEN_CENTRAL

struct peripheral_status_state {
    bool connected;
};

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void set_connection_status(struct peripheral_status_state peripheral) {
    state.connected = peripheral.connected;

    redraw_top();
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            set_connection_status, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

#endif // STATUS_SCREEN_CENTRAL

static struct cat cat;
static atomic_t cat_presses;
static atomic_t cat_last_press_ms;
static atomic_t cat_last_frame_ms;
static enum cat_frame cat_shown_frame = CAT_FRAME_COUNT;
static int cat_shown_position = -1;

static void cat_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cat_work, cat_work_handler);

static void cat_work_handler(struct k_work *work) {
    if (bootloader_shown) {
        return;
    }

    cat_walk(&cat, (uint32_t)atomic_clear(&cat_presses));
    // Read before now, so that a key press in between cannot make idle_ms negative.
    const uint32_t last_press_ms = (uint32_t)atomic_get(&cat_last_press_ms);
    const uint32_t now = k_uptime_get_32();
    uint32_t valid_ms;
    const enum cat_frame frame = cat_frame_at(&cat, now - last_press_ms, &valid_ms);
    if (frame != cat_shown_frame || cat.position != cat_shown_position) {
        screen_show_cat(&screen, frame, cat.position);
        cat_shown_frame = frame;
        cat_shown_position = cat.position;
    }
    atomic_set(&cat_last_frame_ms, (atomic_val_t)now);

    // Does nothing if a key press has rescheduled the work while this handler ran.
    k_work_schedule_for_queue(zmk_display_work_q(), &cat_work, K_MSEC(valid_ms));
}

static int cat_key_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (ev == NULL || !ev->state || !atomic_get(&ready)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    const uint32_t now = k_uptime_get_32();
    atomic_inc(&cat_presses);
    atomic_set(&cat_last_press_ms, (atomic_val_t)now);

    // Every press within one frame period asks for the same deadline, so none postpones it.
    const uint32_t since_frame = now - (uint32_t)atomic_get(&cat_last_frame_ms);
    const uint32_t delay = since_frame >= CAT_FRAME_MS ? 0 : CAT_FRAME_MS - since_frame;
    k_work_reschedule_for_queue(zmk_display_work_q(), &cat_work, K_MSEC(delay));
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_cat, cat_key_listener);
ZMK_SUBSCRIPTION(roamyboard_cat, zmk_position_state_changed);

static void (*bootloader_done)(void);
static enum roamyboard_bootloader_mode bootloader_mode;

static void bootloader_work_handler(struct k_work *work) {
    bootloader_shown = true;
    k_work_cancel_delayable(&cat_work);
    screen_draw_bootloader(&screen, bootloader_mode);
    // The 1-bit flush callback writes to the display before it returns, so the whole view
    // is on the display once lv_refr_now() returns.
    lv_refr_now(NULL);
    LOG_INF("Bootloader view %d is on the display", bootloader_mode);
    bootloader_done();
}

static K_WORK_DEFINE(bootloader_work, bootloader_work_handler);

int roamyboard_status_screen_show_bootloader(enum roamyboard_bootloader_mode mode,
                                             void (*done)(void)) {
    if (!atomic_get(&ready)) {
        return -ENODEV;
    }
    bootloader_mode = mode;
    bootloader_done = done;
    k_work_submit_to_queue(zmk_display_work_q(), &bootloader_work);
    return 0;
}

#if IS_ENABLED(CONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180)

static lv_display_flush_cb_t panel_flush;

static void rotated_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
    lv_area_t panel_area = *area;
    const int err =
        screen_rotate_180(&panel_area, px_map, lv_display_get_horizontal_resolution(display),
                          lv_display_get_vertical_resolution(display));
    if (err) {
        LOG_ERR("Cannot rotate the flushed area (%d,%d)-(%d,%d): %d; showing it unrotated",
                (int)area->x1, (int)area->y1, (int)area->x2, (int)area->y2, err);
    }
    panel_flush(display, &panel_area, px_map);
}

/** Wraps the panel's flush callback, so that every view reaches the panel upside down. */
static void rotate_display(void) {
    lv_display_t *display = lv_display_get_default();
    panel_flush = display->flush_cb;
    lv_display_set_flush_cb(display, rotated_flush);
}

#endif // IS_ENABLED(CONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180)

lv_obj_t *zmk_display_status_screen(void) {
#if IS_ENABLED(CONFIG_ROAMYBOARD_STATUS_SCREEN_ROTATE_180)
    rotate_display();
#endif

    lv_obj_t *root = lv_obj_create(NULL);
    screen_init(&screen, root);

    state.key_module_count = ROAMYBOARD_CHAIN_UNKNOWN;
    screen_draw_top(&screen, &state);
    screen_draw_middle(&screen, &state);
    screen_draw_bottom(&screen, &state);

    widget_battery_status_init();
    widget_chain_status_init();
#if STATUS_SCREEN_CENTRAL
    widget_output_status_init();
#if IS_ENABLED(CONFIG_ROAMYBOARD_HOST_NAMES)
    widget_host_name_status_init();
#endif
    widget_layer_status_init();
#else
    widget_peripheral_status_init();
#endif

    atomic_set(&cat_last_press_ms, (atomic_val_t)k_uptime_get_32());
    atomic_set(&ready, true);
    k_work_schedule_for_queue(zmk_display_work_q(), &cat_work, K_NO_WAIT);

    return root;
}
