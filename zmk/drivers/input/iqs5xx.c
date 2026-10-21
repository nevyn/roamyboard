/*
 * Zephyr input driver for the Azoteq IQS5xx-B000 trackpad on the pointing
 * module: I2C, RDY, probing, configuration and the power policy around the
 * pure logic in iqs5xx_logic.c. docs/firmware.md describes the driver.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT azoteq_iqs5xx

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>

#include "iqs5xx_logic.h"

LOG_MODULE_REGISTER(iqs5xx, CONFIG_ROAMYBOARD_INPUT_IQS5XX_LOG_LEVEL);

#define REG_PRODUCT 0x0000 // product (2), project (2), major, minor
#define REG_SYSTEM_CONTROL_0 0x0431
#define REG_REPORT_RATES 0x057A // Active, Idle-Touch, Idle, LP1, LP2; 2 bytes each
#define REG_IDLE_TIMEOUT 0x0586
#define REG_SYSTEM_CONFIG_0 0x058E
#define REG_SYSTEM_CONFIG_1 0x058F
#define REG_XY_CONFIG_0 0x0669
#define REG_RESOLUTION 0x066E // X (2), Y (2)
#define REG_GESTURE_ENABLES 0x06B7 // single finger, multi finger
#define REG_END_WINDOW 0xEEEE

#define ACK_RESET BIT(7)
#define SETUP_COMPLETE BIT(6)
#define WDT BIT(5)
#define TP_EVENT BIT(2)
#define GESTURE_EVENT BIT(1)
#define EVENT_MODE BIT(0)
#define SWITCH_XY_AXIS BIT(2)
#define FLIP_Y BIT(1)
#define FLIP_X BIT(0)

#define REPORT_RATES 5
#define PROBE_INTERVAL K_MSEC(1000)
#define LIVENESS_INTERVAL K_MSEC(5000)
#define TAP_RELEASE_DELAY K_MSEC(20)
/** Addressing attempts of a forced transfer: a trackpad in a low-power mode NACKs the first. */
#define FORCED_TRIES 3
#define FORCED_RETRY_US 200
#define IDLE_TIMEOUT_NEVER 255
#define IDLE_TIMEOUT_WHILE_ZMK_IDLE_S 1

#if defined(CONFIG_I2C_NRFX_TRANSFER_TIMEOUT) && CONFIG_I2C_NRFX_TRANSFER_TIMEOUT > 0
#define MAX_REPORT_MS (CONFIG_I2C_NRFX_TRANSFER_TIMEOUT / 2)
#else
#define MAX_REPORT_MS UINT16_MAX
#endif

struct iqs5xx_config {
    struct i2c_dt_spec bus;
    struct gpio_dt_spec rdy;
    /** SWITCH_XY_AXIS, FLIP_Y and FLIP_X of XY Config 0. */
    uint8_t xy_config;
    int32_t scroll_divisor;
    /** Active, Idle-Touch, Idle, LP1 and LP2 report intervals in ms. */
    uint16_t report_ms[REPORT_RATES];
};

struct iqs5xx_data {
    const struct device *dev;
    struct gpio_callback rdy_cb;
    /** Reads the frame of a communication window that RDY opened. */
    struct k_work frame_work;
    /** Probes while absent; checks liveness while running. */
    struct k_work_delayable poll_work;
    struct k_work_delayable tap_release_work;
    struct k_work activity_work;
    enum iqs5xx_state state;
    struct iqs5xx_logic logic;
    /** Product number last reported as not an IQS5xx-B000, so it is logged once; -1 for none. */
    int32_t warned_product;
    /** ZMK's activity state: 1 while active, 0 while idle. Written by the event listener. */
    atomic_t zmk_active;
    /** The activity state that the trackpad's Idle timeout was last written for. */
    bool configured_active;
};

static K_THREAD_STACK_DEFINE(iqs5xx_stack, CONFIG_ROAMYBOARD_INPUT_IQS5XX_THREAD_STACK_SIZE);
static struct k_work_q iqs5xx_queue;

static int iqs5xx_write(const struct device *dev, uint16_t reg, const uint8_t *val, size_t len) {
    const struct iqs5xx_config *config = dev->config;
    uint8_t buf[2 + REPORT_RATES * 2];

    __ASSERT_NO_MSG(len <= sizeof(buf) - 2);
    sys_put_be16(reg, buf);
    memcpy(&buf[2], val, len);
    return i2c_write_dt(&config->bus, buf, 2 + len);
}

static int iqs5xx_write_u8(const struct device *dev, uint16_t reg, uint8_t val) {
    return iqs5xx_write(dev, reg, &val, 1);
}

static int iqs5xx_read(const struct device *dev, uint16_t reg, uint8_t *val, size_t len) {
    const struct iqs5xx_config *config = dev->config;
    uint8_t addr[2];

    sys_put_be16(reg, addr);
    return i2c_write_read_dt(&config->bus, addr, sizeof(addr), val, len);
}

/**
 * The first transfer of a communication window. While RDY is low, the trackpad clock-stretches
 * until its next window opens, and in a low-power mode it NACKs the first addressing.
 */
static int iqs5xx_read_forced(const struct device *dev, uint16_t reg, uint8_t *val, size_t len) {
    int err = iqs5xx_read(dev, reg, val, len);

    for (int i = 1; err && i < FORCED_TRIES; i++) {
        k_usleep(FORCED_RETRY_US);
        err = iqs5xx_read(dev, reg, val, len);
    }
    return err;
}

static int iqs5xx_write_u8_forced(const struct device *dev, uint16_t reg, uint8_t val) {
    int err = iqs5xx_write_u8(dev, reg, val);

    for (int i = 1; err && i < FORCED_TRIES; i++) {
        k_usleep(FORCED_RETRY_US);
        err = iqs5xx_write_u8(dev, reg, val);
    }
    return err;
}

/** Ends the communication window; an I2C STOP alone leaves it open. */
static int iqs5xx_end_window(const struct device *dev) {
    return iqs5xx_write_u8(dev, REG_END_WINDOW, 0);
}

static int iqs5xx_step(int err, const char *what, uint16_t reg) {
    if (err) {
        LOG_WRN("Trackpad: %s (0x%04x) failed: %d", what, reg, err);
    }
    return err;
}

static uint8_t iqs5xx_idle_timeout(bool zmk_active) {
    return zmk_active ? IDLE_TIMEOUT_NEVER : IDLE_TIMEOUT_WHILE_ZMK_IDLE_S;
}

static void iqs5xx_report(const struct device *dev, const struct iqs5xx_output *output) {
    for (size_t i = 0; i < output->count; i++) {
        const struct iqs5xx_event *ev = &output->events[i];
        switch (ev->kind) {
        case IQS5XX_EV_REL_X:
            input_report_rel(dev, INPUT_REL_X, ev->value, ev->sync, K_FOREVER);
            break;
        case IQS5XX_EV_REL_Y:
            input_report_rel(dev, INPUT_REL_Y, ev->value, ev->sync, K_FOREVER);
            break;
        case IQS5XX_EV_WHEEL:
            input_report_rel(dev, INPUT_REL_WHEEL, ev->value, ev->sync, K_FOREVER);
            break;
        case IQS5XX_EV_HWHEEL:
            input_report_rel(dev, INPUT_REL_HWHEEL, ev->value, ev->sync, K_FOREVER);
            break;
        case IQS5XX_EV_BUTTON:
            input_report_key(dev, INPUT_BTN_0 + ev->button, ev->value, ev->sync, K_FOREVER);
            break;
        }
    }
}

static const char *iqs5xx_product_name(uint16_t product) {
    switch (product) {
    case 40:
        return "IQS550";
    case 52:
        return "IQS525";
    case 58:
        return "IQS572";
    }
    return "unknown";
}

/** Reads the Product number; ends the window only for an IQS5xx-B000. */
static int iqs5xx_probe(const struct device *dev, uint16_t *product) {
    const struct iqs5xx_config *config = dev->config;
    uint8_t id[2];

    int err = iqs5xx_read_forced(dev, REG_PRODUCT, id, sizeof(id));
    if (err) {
        LOG_DBG("No trackpad at 0x%02x on %s: %d", config->bus.addr, config->bus.bus->name, err);
        return err;
    }
    *product = sys_get_be16(id);
    if (!iqs5xx_product_known(*product)) {
        return 0;
    }
    return iqs5xx_step(iqs5xx_end_window(dev), "ending the probe's window", REG_END_WINDOW);
}

/** Writes the whole configuration in one communication window and ends it. */
static int iqs5xx_configure(const struct device *dev) {
    const struct iqs5xx_config *config = dev->config;
    struct iqs5xx_data *data = dev->data;
    uint8_t buf[REPORT_RATES * 2];
    uint8_t id[6];
    uint8_t resolution[4];
    uint8_t reg;
    int err;

    err = iqs5xx_write_u8_forced(dev, REG_SYSTEM_CONTROL_0, ACK_RESET);
    if (iqs5xx_step(err, "writing ACK_RESET to System Control 0", REG_SYSTEM_CONTROL_0)) {
        return err;
    }

    err = iqs5xx_write_u8(dev, REG_SYSTEM_CONFIG_1, EVENT_MODE | GESTURE_EVENT | TP_EVENT);
    if (iqs5xx_step(err, "writing System Config 1", REG_SYSTEM_CONFIG_1)) {
        return err;
    }

    const uint8_t gestures[2] = {IQS5XX_SINGLE_TAP | IQS5XX_PRESS_AND_HOLD,
                                 IQS5XX_TWO_FINGER_TAP | IQS5XX_SCROLL};
    err = iqs5xx_write(dev, REG_GESTURE_ENABLES, gestures, sizeof(gestures));
    if (iqs5xx_step(err, "writing the gesture enables", REG_GESTURE_ENABLES)) {
        return err;
    }

    // Read-modify-write keeps PALM_REJECT from the trackpad's own settings.
    err = iqs5xx_read(dev, REG_XY_CONFIG_0, &reg, 1);
    if (iqs5xx_step(err, "reading XY Config 0", REG_XY_CONFIG_0)) {
        return err;
    }
    reg = (reg & ~(SWITCH_XY_AXIS | FLIP_Y | FLIP_X)) | config->xy_config;
    err = iqs5xx_write_u8(dev, REG_XY_CONFIG_0, reg);
    if (iqs5xx_step(err, "writing XY Config 0", REG_XY_CONFIG_0)) {
        return err;
    }

    for (int i = 0; i < REPORT_RATES; i++) {
        sys_put_be16(config->report_ms[i], &buf[i * 2]);
    }
    err = iqs5xx_write(dev, REG_REPORT_RATES, buf, sizeof(buf));
    if (iqs5xx_step(err, "writing the report rates", REG_REPORT_RATES)) {
        return err;
    }

    const bool zmk_active = atomic_get(&data->zmk_active);
    err = iqs5xx_write_u8(dev, REG_IDLE_TIMEOUT, iqs5xx_idle_timeout(zmk_active));
    if (iqs5xx_step(err, "writing the Idle timeout", REG_IDLE_TIMEOUT)) {
        return err;
    }

    err = iqs5xx_read(dev, REG_PRODUCT, id, sizeof(id));
    if (iqs5xx_step(err, "reading the product and version", REG_PRODUCT)) {
        return err;
    }
    err = iqs5xx_read(dev, REG_RESOLUTION, resolution, sizeof(resolution));
    if (iqs5xx_step(err, "reading the resolution", REG_RESOLUTION)) {
        return err;
    }

    // Read-modify-write keeps the Re-ATI and switch input settings.
    err = iqs5xx_read(dev, REG_SYSTEM_CONFIG_0, &reg, 1);
    if (iqs5xx_step(err, "reading System Config 0", REG_SYSTEM_CONFIG_0)) {
        return err;
    }
    err = iqs5xx_write_u8(dev, REG_SYSTEM_CONFIG_0, reg | SETUP_COMPLETE | WDT);
    if (iqs5xx_step(err, "writing System Config 0", REG_SYSTEM_CONFIG_0)) {
        return err;
    }

    err = iqs5xx_end_window(dev);
    if (iqs5xx_step(err, "ending the configuration's window", REG_END_WINDOW)) {
        return err;
    }

    data->configured_active = zmk_active;
    const uint16_t product = sys_get_be16(&id[0]);
    LOG_INF("Trackpad %s (product %u), version %u.%u, resolution %u x %u, ZMK %s",
            iqs5xx_product_name(product), product, id[4], id[5], sys_get_be16(&resolution[0]),
            sys_get_be16(&resolution[2]), zmk_active ? "active" : "idle");
    return 0;
}

static void iqs5xx_set_rdy_interrupt(const struct device *dev, bool enabled) {
    const struct iqs5xx_config *config = dev->config;

    const int err = gpio_pin_interrupt_configure_dt(
        &config->rdy, enabled ? GPIO_INT_EDGE_TO_ACTIVE : GPIO_INT_DISABLE);
    if (err) {
        LOG_ERR("Unable to %s the RDY interrupt on %s pin %u: %d", enabled ? "enable" : "disable",
                config->rdy.port->name, config->rdy.pin, err);
    }
}

/** Feeds an input to the state machine and carries out the transitions that follow. */
static void iqs5xx_feed(const struct device *dev, enum iqs5xx_input input, uint16_t product) {
    const struct iqs5xx_config *config = dev->config;
    struct iqs5xx_data *data = dev->data;

    for (;;) {
        const struct iqs5xx_transition t = iqs5xx_next(data->state, input, product);
        if (t.next != data->state) {
            LOG_INF("Trackpad %s -> %s", iqs5xx_state_name(data->state),
                    iqs5xx_state_name(t.next));
        }
        data->state = t.next;

        if (t.actions & IQS5XX_DO_DISABLE_RDY) {
            iqs5xx_set_rdy_interrupt(dev, false);
            k_work_cancel(&data->frame_work);
        }
        if (t.actions & IQS5XX_DO_RELEASE) {
            struct iqs5xx_output output;
            k_work_cancel_delayable(&data->tap_release_work);
            iqs5xx_logic_release_all(&data->logic, &output);
            iqs5xx_report(dev, &output);
        }
        if ((t.actions & IQS5XX_DO_WARN_PRODUCT) && product != data->warned_product) {
            LOG_WRN("The device at 0x%02x on %s has product number %u, not an IQS5xx-B000 "
                    "(40, 52 or 58); the trackpad driver ignores it",
                    config->bus.addr, config->bus.bus->name, product);
            data->warned_product = product;
        }
        if (t.actions & IQS5XX_DO_PROBE_LATER) {
            k_work_reschedule_for_queue(&iqs5xx_queue, &data->poll_work, PROBE_INTERVAL);
        }
        if (t.actions & IQS5XX_DO_CHECK_LATER) {
            k_work_reschedule_for_queue(&iqs5xx_queue, &data->poll_work, LIVENESS_INTERVAL);
        }
        if (t.actions & IQS5XX_DO_ENABLE_RDY) {
            iqs5xx_set_rdy_interrupt(dev, true);
            if (gpio_pin_get_dt(&config->rdy) > 0) {
                k_work_submit_to_queue(&iqs5xx_queue, &data->frame_work);
            }
        }
        if (!(t.actions & IQS5XX_DO_CONFIGURE)) {
            return;
        }
        data->warned_product = -1;
        input = iqs5xx_configure(dev) ? IQS5XX_IN_IO_ERROR : IQS5XX_IN_CONFIGURED;
    }
}

/**
 * Reads and handles one communication window's frame, applies a pending power change in the
 * same window and ends it. Opens the window by force if RDY is low.
 */
static void iqs5xx_service_window(const struct device *dev) {
    struct iqs5xx_data *data = dev->data;
    uint8_t raw[IQS5XX_FRAME_LEN];
    struct iqs5xx_frame frame;
    struct iqs5xx_output output;

    int err = iqs5xx_read_forced(dev, IQS5XX_FRAME_START, raw, sizeof(raw));
    if (iqs5xx_step(err, "reading the frame", IQS5XX_FRAME_START)) {
        iqs5xx_feed(dev, IQS5XX_IN_IO_ERROR, 0);
        return;
    }
    LOG_HEXDUMP_DBG(raw, sizeof(raw), "trackpad frame 0x000D..0x0015");

    iqs5xx_frame_decode(raw, &frame);
    if (frame.system_info0 & IQS5XX_SHOW_RESET) {
        LOG_INF("Trackpad reset; configuring it again");
        iqs5xx_feed(dev, IQS5XX_IN_SHOW_RESET, 0);
        return;
    }
    iqs5xx_logic_frame(&data->logic, &frame, &output);

    const bool zmk_active = atomic_get(&data->zmk_active);
    if (zmk_active != data->configured_active) {
        err = iqs5xx_write_u8(dev, REG_IDLE_TIMEOUT, iqs5xx_idle_timeout(zmk_active));
        if (!iqs5xx_step(err, "writing the Idle timeout", REG_IDLE_TIMEOUT)) {
            data->configured_active = zmk_active;
            LOG_DBG("Trackpad Idle timeout set for ZMK %s", zmk_active ? "active" : "idle");
        }
    }
    if (!err) {
        err = iqs5xx_step(iqs5xx_end_window(dev), "ending the window", REG_END_WINDOW);
    }

    iqs5xx_report(dev, &output);
    if (output.tap_release_due) {
        k_work_reschedule_for_queue(&iqs5xx_queue, &data->tap_release_work, TAP_RELEASE_DELAY);
    }
    if (err) {
        iqs5xx_feed(dev, IQS5XX_IN_IO_ERROR, 0);
        return;
    }
    k_work_reschedule_for_queue(&iqs5xx_queue, &data->poll_work, LIVENESS_INTERVAL);
}

static void iqs5xx_frame_work_handler(struct k_work *work) {
    struct iqs5xx_data *data = CONTAINER_OF(work, struct iqs5xx_data, frame_work);
    const struct device *dev = data->dev;
    const struct iqs5xx_config *config = dev->config;

    // A forced transfer may have served and ended the window that raised this edge.
    if (data->state != IQS5XX_RUNNING || gpio_pin_get_dt(&config->rdy) <= 0) {
        return;
    }
    iqs5xx_service_window(dev);
}

static void iqs5xx_poll_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct iqs5xx_data *data = CONTAINER_OF(dwork, struct iqs5xx_data, poll_work);
    const struct device *dev = data->dev;
    uint16_t product = 0;

    switch (data->state) {
    case IQS5XX_ABSENT: {
        const int err = iqs5xx_probe(dev, &product);
        iqs5xx_feed(dev, err ? IQS5XX_IN_PROBE_FAILED : IQS5XX_IN_PROBE_OK, product);
        break;
    }
    case IQS5XX_RUNNING:
        iqs5xx_service_window(dev);
        break;
    case IQS5XX_CONFIGURING:
        break;
    }
}

static void iqs5xx_tap_release_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct iqs5xx_data *data = CONTAINER_OF(dwork, struct iqs5xx_data, tap_release_work);
    struct iqs5xx_output output;

    iqs5xx_logic_release_taps(&data->logic, &output);
    iqs5xx_report(data->dev, &output);
}

static void iqs5xx_activity_work_handler(struct k_work *work) {
    struct iqs5xx_data *data = CONTAINER_OF(work, struct iqs5xx_data, activity_work);

    if (data->state == IQS5XX_RUNNING &&
        (bool)atomic_get(&data->zmk_active) != data->configured_active) {
        iqs5xx_service_window(data->dev);
    }
}

static void iqs5xx_rdy_handler(const struct device *port, struct gpio_callback *cb,
                               gpio_port_pins_t pins) {
    struct iqs5xx_data *data = CONTAINER_OF(cb, struct iqs5xx_data, rdy_cb);

    k_work_submit_to_queue(&iqs5xx_queue, &data->frame_work);
}

static int iqs5xx_init(const struct device *dev) {
    const struct iqs5xx_config *config = dev->config;
    struct iqs5xx_data *data = dev->data;
    static bool queue_started;

    data->dev = dev;
    data->state = IQS5XX_ABSENT;
    data->warned_product = -1;
    iqs5xx_logic_init(&data->logic, config->scroll_divisor);
    atomic_set(&data->zmk_active, 1);

    if (!i2c_is_ready_dt(&config->bus)) {
        LOG_ERR("I2C bus %s is not ready", config->bus.bus->name);
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&config->rdy)) {
        LOG_ERR("GPIO %s for RDY is not ready", config->rdy.port->name);
        return -ENODEV;
    }
    int err = gpio_pin_configure_dt(&config->rdy, GPIO_INPUT);
    if (err) {
        LOG_ERR("Unable to configure RDY pin %u on %s: %d", config->rdy.pin,
                config->rdy.port->name, err);
        return err;
    }
    gpio_init_callback(&data->rdy_cb, iqs5xx_rdy_handler, BIT(config->rdy.pin));
    err = gpio_add_callback(config->rdy.port, &data->rdy_cb);
    if (err) {
        LOG_ERR("Unable to add the RDY callback on %s: %d", config->rdy.port->name, err);
        return err;
    }

    if (!queue_started) {
        const struct k_work_queue_config queue_config = {.name = "iqs5xx"};
        k_work_queue_start(&iqs5xx_queue, iqs5xx_stack, K_THREAD_STACK_SIZEOF(iqs5xx_stack),
                           K_PRIO_PREEMPT(CONFIG_ROAMYBOARD_INPUT_IQS5XX_THREAD_PRIORITY),
                           &queue_config);
        queue_started = true;
    }

    k_work_init(&data->frame_work, iqs5xx_frame_work_handler);
    k_work_init_delayable(&data->poll_work, iqs5xx_poll_work_handler);
    k_work_init_delayable(&data->tap_release_work, iqs5xx_tap_release_work_handler);
    k_work_init(&data->activity_work, iqs5xx_activity_work_handler);
    k_work_reschedule_for_queue(&iqs5xx_queue, &data->poll_work, K_NO_WAIT);
    return 0;
}

#define INST_REPORT_MS_OK(n, prop)                                                                 \
    BUILD_ASSERT(DT_INST_PROP(n, prop) >= 1 && DT_INST_PROP(n, prop) <= MAX_REPORT_MS,             \
                 #prop " must be 1 to half of CONFIG_I2C_NRFX_TRANSFER_TIMEOUT")

#define IQS5XX_INIT(n)                                                                             \
    BUILD_ASSERT(DT_INST_PROP(n, scroll_divisor) >= 1, "scroll-divisor must be at least 1");       \
    INST_REPORT_MS_OK(n, active_report_ms);                                                        \
    INST_REPORT_MS_OK(n, idle_report_ms);                                                          \
    INST_REPORT_MS_OK(n, lp1_report_ms);                                                           \
    INST_REPORT_MS_OK(n, lp2_report_ms);                                                           \
                                                                                                   \
    static struct iqs5xx_data iqs5xx_data_##n;                                                     \
                                                                                                   \
    static const struct iqs5xx_config iqs5xx_config_##n = {                                        \
        .bus = I2C_DT_SPEC_INST_GET(n),                                                            \
        .rdy = GPIO_DT_SPEC_INST_GET(n, rdy_gpios),                                                \
        .xy_config = (DT_INST_PROP(n, switch_xy) ? SWITCH_XY_AXIS : 0) |                           \
                     (DT_INST_PROP(n, flip_y) ? FLIP_Y : 0) |                                      \
                     (DT_INST_PROP(n, flip_x) ? FLIP_X : 0),                                       \
        .scroll_divisor = DT_INST_PROP(n, scroll_divisor),                                         \
        .report_ms =                                                                               \
            {                                                                                      \
                DT_INST_PROP(n, active_report_ms),                                                 \
                DT_INST_PROP(n, active_report_ms),                                                 \
                DT_INST_PROP(n, idle_report_ms),                                                   \
                DT_INST_PROP(n, lp1_report_ms),                                                    \
                DT_INST_PROP(n, lp2_report_ms),                                                    \
            },                                                                                     \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, iqs5xx_init, NULL, &iqs5xx_data_##n, &iqs5xx_config_##n, POST_KERNEL, \
                          CONFIG_ROAMYBOARD_INPUT_IQS5XX_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(IQS5XX_INIT)

#define INST_DATA_PTR(n) &iqs5xx_data_##n,

static struct iqs5xx_data *const iqs5xx_instances[] = {DT_INST_FOREACH_STATUS_OKAY(INST_DATA_PTR)};

static int iqs5xx_activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (!ev) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    const bool active = ev->state == ZMK_ACTIVITY_ACTIVE;
    for (size_t i = 0; i < ARRAY_SIZE(iqs5xx_instances); i++) {
        atomic_set(&iqs5xx_instances[i]->zmk_active, active);
        k_work_submit_to_queue(&iqs5xx_queue, &iqs5xx_instances[i]->activity_work);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(iqs5xx, iqs5xx_activity_listener);
ZMK_SUBSCRIPTION(iqs5xx, zmk_activity_state_changed);
