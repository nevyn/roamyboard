/*
 *
 * Copyright (c) 2023 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include <roamyboard/build_id.h>
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
#include <dt-bindings/zmk/pointing.h>
#include <zmk/ble.h>
#include <zmk/behavior.h>
#include <zmk/endpoints.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
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

    if (!bootloader_shown) {
        screen_draw_top(&screen, &state);
        screen_draw_middle(&screen, &state);
    }
}

#if IS_ENABLED(CONFIG_ROAMYBOARD_LOCAL_OTA)
#define LOCAL_OTA_HOLD_DS (CONFIG_ROAMYBOARD_LOCAL_OTA_HOLD_MS / 100)

/** "hold the\n0 key 3 s\nfor OTA", with tenths when the hold time needs them. */
static char local_ota_hint[48];

static void format_local_ota_hint(void) {
    if (LOCAL_OTA_HOLD_DS % 10 == 0) {
        snprintf(local_ota_hint, sizeof(local_ota_hint), "hold the\n%s key %d s\nfor OTA",
                 CONFIG_ROAMYBOARD_LOCAL_OTA_LABEL, LOCAL_OTA_HOLD_DS / 10);
    } else {
        snprintf(local_ota_hint, sizeof(local_ota_hint), "hold the\n%s key %d.%d s\nfor OTA",
                 CONFIG_ROAMYBOARD_LOCAL_OTA_LABEL, LOCAL_OTA_HOLD_DS / 10, LOCAL_OTA_HOLD_DS % 10);
    }
    state.local_ota_hint = local_ota_hint;
}
#endif

ZMK_DISPLAY_WIDGET_LISTENER(widget_peripheral_status, struct peripheral_status_state,
                            set_connection_status, get_state)
ZMK_SUBSCRIPTION(widget_peripheral_status, zmk_split_peripheral_status_changed);

#endif // STATUS_SCREEN_CENTRAL

#if STATUS_SCREEN_CENTRAL

/** Shortest time between two redraws of the cat; events in between are shown together. */
#define CAT_FRAME_MS 40

/** Where the cat stands when it appears. */
#define CAT_START_X 2

// Layers with a pose, by node label in the keymap; ZMK's layer ID is the layer's child index.
#define CAT_LAYER_ID(label)                                                                        \
    COND_CODE_1(DT_NODE_EXISTS(DT_NODELABEL(label)), (DT_NODE_CHILD_IDX(DT_NODELABEL(label))), (-1))
#define CAT_LAYER_IS_IN_KEYMAP(node) DT_SAME_NODE(DT_PARENT(node), DT_INST(0, zmk_keymap))
#define CAT_LAYER_IN_KEYMAP(label)                                                                 \
    BUILD_ASSERT(COND_CODE_1(DT_NODE_EXISTS(DT_NODELABEL(label)),                                  \
                             (CAT_LAYER_IS_IN_KEYMAP(DT_NODELABEL(label))), (1)),                  \
                 #label " must label a layer of the keymap")

CAT_LAYER_IN_KEYMAP(keypad_layer);
CAT_LAYER_IN_KEYMAP(system_layer);
CAT_LAYER_IN_KEYMAP(gaming_layer);
CAT_LAYER_IN_KEYMAP(locked_layer);

enum cat_event_kind {
    CAT_EVENT_KEY,
    CAT_EVENT_MOUSE_PRESSED,
    CAT_EVENT_MOUSE_RELEASED,
    CAT_EVENT_POSE,
    CAT_EVENT_PROFILE,
};

struct cat_event {
    uint32_t ms;
    uint8_t kind;
    /** enum cat_key, enum cat_mouse or enum cat_pose, by kind. */
    uint8_t arg;
};

// Events reach the display work queue through a queue, since ZMK raises them on other threads.
K_MSGQ_DEFINE(cat_events, sizeof(struct cat_event), 32, 4);

static struct cat cat;
static struct cat_view cat_shown = {.frame = CAT_FRAME_COUNT};
static atomic_t cat_last_frame_ms;

/** The cat appears with the key module count, once the startup build ID is gone. */
static atomic_t cat_started;

/** Position of each held mouse key, by enum cat_mouse, or -1; its release ends the stalk. */
static atomic_t cat_mouse_positions[CAT_MOUSE_CLICK + 1] = {
    ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1), ATOMIC_INIT(-1)};

static void cat_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(cat_work, cat_work_handler);

static enum cat_pose cat_pose_of_layer(zmk_keymap_layer_id_t layer) {
    if (layer == CAT_LAYER_ID(keypad_layer)) {
        return CAT_POSE_KEYPAD;
    }
    if (layer == CAT_LAYER_ID(system_layer)) {
        return CAT_POSE_SYSTEM;
    }
    if (layer == CAT_LAYER_ID(gaming_layer)) {
        return CAT_POSE_GAMING;
    }
    if (layer == CAT_LAYER_ID(locked_layer)) {
        return CAT_POSE_LOCKED;
    }
    return CAT_POSE_NONE;
}

static enum cat_pose cat_current_pose(void) {
    return cat_pose_of_layer(zmk_keymap_layer_index_to_id(zmk_keymap_highest_layer_active()));
}

static void cat_feed(const struct cat_event *ev) {
    switch (ev->kind) {
    case CAT_EVENT_KEY:
        cat_key_pressed(&cat, (enum cat_key)ev->arg, ev->ms);
        break;
    case CAT_EVENT_MOUSE_PRESSED:
        cat_mouse_pressed(&cat, (enum cat_mouse)ev->arg, ev->ms);
        break;
    case CAT_EVENT_MOUSE_RELEASED:
        cat_mouse_released(&cat, (enum cat_mouse)ev->arg, ev->ms);
        break;
    case CAT_EVENT_POSE:
        cat_set_pose(&cat, (enum cat_pose)ev->arg, ev->ms);
        break;
    case CAT_EVENT_PROFILE:
        cat_profile_changed(&cat, ev->ms);
        break;
    default:
        LOG_ERR("Unknown cat event kind %d", ev->kind);
        break;
    }
}

static void cat_work_handler(struct k_work *work) {
    if (bootloader_shown) {
        return;
    }

    struct cat_event ev;
    while (k_msgq_get(&cat_events, &ev, K_NO_WAIT) == 0) {
        cat_feed(&ev);
    }
    // Read after the events, so that no event lies after now.
    const uint32_t now = k_uptime_get_32();
    const struct cat_view view = cat_view_at(&cat, now);
    if (view.frame != cat_shown.frame || view.x != cat_shown.x || view.y != cat_shown.y) {
        screen_show_cat(&screen, view.frame, view.x, view.y);
        cat_shown = view;
    }
    atomic_set(&cat_last_frame_ms, (atomic_val_t)now);

    // Does nothing if an event has rescheduled the work while this handler ran.
    k_work_schedule_for_queue(zmk_display_work_q(), &cat_work, K_MSEC(view.valid_ms));
}

static void cat_post(enum cat_event_kind kind, int arg) {
    if (!atomic_get(&cat_started)) {
        return;
    }
    const uint32_t now = k_uptime_get_32();
    const struct cat_event ev = {.ms = now, .kind = kind, .arg = (uint8_t)arg};
    if (k_msgq_put(&cat_events, &ev, K_NO_WAIT) != 0) {
        LOG_WRN("Cat event queue full; dropped event kind %d, arg %d", kind, arg);
        return;
    }

    // Every event within one frame period asks for the same deadline, so none postpones it.
    const uint32_t since_frame = now - (uint32_t)atomic_get(&cat_last_frame_ms);
    const uint32_t delay = since_frame >= CAT_FRAME_MS ? 0 : CAT_FRAME_MS - since_frame;
    k_work_reschedule_for_queue(zmk_display_work_q(), &cat_work, K_MSEC(delay));
}

static bool binding_is(const struct zmk_behavior_binding *binding, const char *behavior) {
    return binding->behavior_dev != NULL && strcmp(binding->behavior_dev, behavior) == 0;
}

/** The binding that a press at position triggers: the highest active one that is not &trans. */
static const struct zmk_behavior_binding *active_binding_at(uint32_t position) {
    const zmk_keymap_layer_id_t base = zmk_keymap_layer_default();
    for (int index = ZMK_KEYMAP_LAYERS_LEN - 1; index >= 0; index--) {
        const zmk_keymap_layer_id_t layer = zmk_keymap_layer_index_to_id(index);
        if (layer == ZMK_KEYMAP_LAYER_ID_INVAL || !zmk_keymap_layer_active(layer)) {
            continue;
        }
        const struct zmk_behavior_binding *binding =
            zmk_keymap_get_layer_binding_at_idx(layer, position);
        if (binding == NULL) {
            return NULL;
        }
        if (!binding_is(binding, DEVICE_DT_NAME(DT_NODELABEL(trans))) || layer == base) {
            return binding;
        }
    }
    return NULL;
}

/** Mouse keys raise no keycode event, so the cat looks up the binding: enum cat_mouse, or -1. */
static int mouse_key_at(uint32_t position) {
    const struct zmk_behavior_binding *binding = active_binding_at(position);
    if (binding == NULL) {
        return -1;
    }
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(mkp))
    if (binding_is(binding, DEVICE_DT_NAME(DT_NODELABEL(mkp)))) {
        return CAT_MOUSE_CLICK;
    }
#endif
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(mmv))
    if (binding_is(binding, DEVICE_DT_NAME(DT_NODELABEL(mmv)))) {
        const int x = MOVE_X_DECODE(binding->param1);
        const int y = MOVE_Y_DECODE(binding->param1);
        if (abs(x) >= abs(y)) {
            return x < 0 ? CAT_MOUSE_LEFT : CAT_MOUSE_RIGHT;
        }
        return y < 0 ? CAT_MOUSE_UP : CAT_MOUSE_DOWN;
    }
#endif
    return -1;
}

static void cat_position_changed(const struct zmk_position_state_changed *ev) {
    if (ev->state) {
        const int mouse = mouse_key_at(ev->position);
        if (mouse >= 0) {
            atomic_set(&cat_mouse_positions[mouse], (atomic_val_t)ev->position);
            cat_post(CAT_EVENT_MOUSE_PRESSED, mouse);
        }
        return;
    }
    for (int mouse = 0; mouse <= CAT_MOUSE_CLICK; mouse++) {
        if (atomic_cas(&cat_mouse_positions[mouse], (atomic_val_t)ev->position, -1)) {
            cat_post(CAT_EVENT_MOUSE_RELEASED, mouse);
        }
    }
}

static int cat_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *keycode = as_zmk_keycode_state_changed(eh);
    if (keycode != NULL) {
        if (keycode->state) {
            cat_post(CAT_EVENT_KEY, cat_classify_key(keycode->usage_page, keycode->keycode));
        }
        return ZMK_EV_EVENT_BUBBLE;
    }
    const struct zmk_position_state_changed *position = as_zmk_position_state_changed(eh);
    if (position != NULL) {
        cat_position_changed(position);
        return ZMK_EV_EVENT_BUBBLE;
    }
    if (as_zmk_layer_state_changed(eh) != NULL) {
        cat_post(CAT_EVENT_POSE, cat_current_pose());
        return ZMK_EV_EVENT_BUBBLE;
    }
#if defined(CONFIG_ZMK_BLE)
    if (as_zmk_ble_active_profile_changed(eh) != NULL) {
        cat_post(CAT_EVENT_PROFILE, 0);
    }
#endif
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(roamyboard_cat, cat_listener);
ZMK_SUBSCRIPTION(roamyboard_cat, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(roamyboard_cat, zmk_position_state_changed);
ZMK_SUBSCRIPTION(roamyboard_cat, zmk_layer_state_changed);
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(roamyboard_cat, zmk_ble_active_profile_changed);
#endif

static void cat_start(void) {
    const uint32_t now = k_uptime_get_32();
    cat_init(&cat, CAT_START_X, now);
    cat_set_pose(&cat, cat_current_pose(), now);
    atomic_set(&cat_started, true);
    k_work_schedule_for_queue(zmk_display_work_q(), &cat_work, K_NO_WAIT);
}

#endif // STATUS_SCREEN_CENTRAL

/** How long the count box shows the build ID after the screen starts. */
#define STARTUP_BUILD_ID_MS 3000

static void startup_end_handler(struct k_work *work) {
    if (bootloader_shown) {
        return;
    }
    state.startup_build_id = NULL;
    screen_draw_top(&screen, &state);
#if STATUS_SCREEN_CENTRAL
    cat_start();
#endif
}

static K_WORK_DELAYABLE_DEFINE(startup_end_work, startup_end_handler);

static void (*bootloader_done)(void);
static enum roamyboard_bootloader_mode bootloader_mode;

static void bootloader_work_handler(struct k_work *work) {
    bootloader_shown = true;
#if STATUS_SCREEN_CENTRAL
    k_work_cancel_delayable(&cat_work);
#endif
    screen_draw_bootloader(&screen, bootloader_mode, roamyboard_build_id());
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
    state.startup_build_id = roamyboard_build_id();
#if IS_ENABLED(CONFIG_ROAMYBOARD_LOCAL_OTA)
    format_local_ota_hint();
#endif
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

    atomic_set(&ready, true);
    k_work_schedule_for_queue(zmk_display_work_q(), &startup_end_work,
                              K_MSEC(STARTUP_BUILD_ID_MS));

    return root;
}
