/*
 * Zephyr kscan driver for roamyboard's 74HC165 chain: SPI, /PL, polling and
 * debouncing around the pure logic in chain.c.
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT roamyboard_kscan_165_chain

#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/kscan.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>

#include <roamyboard/chain_state.h>
#include <roamyboard/events/chain_state_changed.h>
#include <zmk/debounce.h>

#include "chain.h"

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define FAULT_LOG_INTERVAL_MS 5000
/** Leading chain bytes that the debug log dumps whenever they change. */
#define DEBUG_DUMP_LEN 4

BUILD_ASSERT(CHAIN_ANCHOR_MCU == 0 && CHAIN_ANCHOR_TERMINATOR == 1,
             "chain_anchor must follow the order of the anchor enum in the binding");
BUILD_ASSERT(CHAIN_COUNT_FAULT == ROAMYBOARD_CHAIN_FAULT &&
                 CHAIN_COUNT_UNKNOWN == ROAMYBOARD_CHAIN_UNKNOWN,
             "chain.h and roamyboard/chain_state.h must agree on the special counts");
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) <= 1,
             "roamyboard_chain_key_module_count() reports a single chain per MCU module");

static atomic_t accepted_key_module_count = ATOMIC_INIT(CHAIN_COUNT_UNKNOWN);

int roamyboard_chain_key_module_count(void) {
    return (int)atomic_get(&accepted_key_module_count);
}

struct kscan_chain_config {
    struct spi_dt_spec bus;
    struct gpio_dt_spec load;
    struct chain_config chain;
    struct zmk_debounce_config debounce;
    size_t read_len;
    size_t keys;
    int32_t debounce_scan_period_ms;
    int32_t poll_period_ms;
};

struct kscan_chain_data {
    const struct device *dev;
    kscan_callback_t callback;
    struct k_work_delayable work;
    /** Timestamp of the current or scheduled scan. */
    int64_t scan_time;
    struct chain chain;
    /** read_len bytes from the last read. */
    uint8_t *buf;
    /** keys entries each, row-major in keymap coordinates. */
    bool *active;
    bool *pressed;
    bool *reported;
    struct zmk_debounce_state *debounce;
    int64_t next_fault_log;
    uint8_t last_dump[DEBUG_DUMP_LEN];
    unsigned int faults_since_log;
};

/** Counts a faulty scan; returns whether one is due for logging, which restarts the interval. */
static bool kscan_chain_fault_log_due(struct kscan_chain_data *data) {
    data->faults_since_log++;
    const int64_t now = k_uptime_get();
    if (now < data->next_fault_log) {
        return false;
    }
    data->next_fault_log = now + FAULT_LOG_INTERVAL_MS;
    return true;
}

static void kscan_chain_log_read_error(const struct device *dev, int err) {
    const struct kscan_chain_config *config = dev->config;
    struct kscan_chain_data *data = dev->data;

    if (!kscan_chain_fault_log_due(data)) {
        return;
    }
    LOG_ERR("Chain read on %s failed: %d (%u failed scans since the last report)",
            config->bus.bus->name, err, data->faults_since_log);
    data->faults_since_log = 0;
}

static void kscan_chain_log_missing_sentinel(const struct device *dev) {
    const struct kscan_chain_config *config = dev->config;
    struct kscan_chain_data *data = dev->data;

    if (!kscan_chain_fault_log_due(data)) {
        return;
    }
    LOG_WRN("No sentinel in the %u bytes read from the chain (%u faulty scans since the last "
            "report, accepted key module count %d); check the terminator module and DATA",
            (unsigned int)config->read_len, data->faults_since_log, data->chain.accepted);
    LOG_HEXDUMP_WRN(data->buf, config->read_len, "chain bytes, nearest key module first");
    data->faults_since_log = 0;
}

static void kscan_chain_emit(void *ctx, int row, int column, bool pressed) {
    const struct device *dev = ctx;
    struct kscan_chain_data *data = dev->data;

    LOG_DBG("Sending event at %i,%i state %s", row, column, pressed ? "on" : "off");
    data->callback(dev, row, column, pressed);
}

static int kscan_chain_read(const struct device *dev) {
    const struct kscan_chain_config *config = dev->config;
    struct kscan_chain_data *data = dev->data;

    int err = gpio_pin_set_dt(&config->load, 1);
    if (err) {
        return err;
    }
    k_busy_wait(1);
    err = gpio_pin_set_dt(&config->load, 0);
    if (err) {
        return err;
    }

    const struct spi_buf rx_buf = {.buf = data->buf, .len = config->read_len};
    const struct spi_buf_set rx = {.buffers = &rx_buf, .count = 1};
    return spi_read_dt(&config->bus, &rx);
}

/** Handles one read; returns whether the next scan should come after the short period. */
static bool kscan_chain_process(const struct device *dev) {
    const struct kscan_chain_config *config = dev->config;
    struct kscan_chain_data *data = dev->data;

    const int err = kscan_chain_read(dev);
    if (err) {
        kscan_chain_log_read_error(dev, err);
        return false;
    }

    const size_t dump_len = MIN(config->read_len, DEBUG_DUMP_LEN);
    if (CONFIG_ZMK_LOG_LEVEL >= LOG_LEVEL_DBG && memcmp(data->last_dump, data->buf, dump_len)) {
        memcpy(data->last_dump, data->buf, dump_len);
        LOG_HEXDUMP_DBG(data->buf, dump_len, "chain bytes changed, nearest key module first");
    }

    const enum chain_scan_result result =
        chain_scan(&data->chain, data->buf, config->read_len, data->active);
    if (data->chain.observed == CHAIN_COUNT_FAULT) {
        kscan_chain_log_missing_sentinel(dev);
    }

    switch (result) {
    case CHAIN_SCAN_REMAP:
        if (data->chain.accepted == CHAIN_COUNT_FAULT) {
            LOG_WRN("Chain has had no sentinel for %d scans; releasing all keys",
                    config->chain.stable_scans);
        } else {
            LOG_INF("Chain has %d key modules", data->chain.accepted);
        }
        chain_release_all(&data->chain, kscan_chain_emit, (void *)dev);
        memset(data->debounce, 0, config->keys * sizeof(data->debounce[0]));
        atomic_set(&accepted_key_module_count, data->chain.accepted);
        raise_roamyboard_chain_state_changed(
            (struct roamyboard_chain_state_changed){.key_module_count = data->chain.accepted});
        return true;

    case CHAIN_SCAN_SETTLING:
    case CHAIN_SCAN_FAULT:
        return false;

    case CHAIN_SCAN_KEYS:
        break;
    }

    bool busy = false;
    for (size_t i = 0; i < config->keys; i++) {
        struct zmk_debounce_state *state = &data->debounce[i];
        zmk_debounce_update(state, data->active[i], config->debounce_scan_period_ms,
                            &config->debounce);
        data->pressed[i] = zmk_debounce_is_pressed(state);
        busy = busy || zmk_debounce_is_active(state);
    }
    chain_report(&data->chain, data->pressed, kscan_chain_emit, (void *)dev);
    return busy;
}

static void kscan_chain_work_handler(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct kscan_chain_data *data = CONTAINER_OF(dwork, struct kscan_chain_data, work);
    const struct device *dev = data->dev;
    const struct kscan_chain_config *config = dev->config;

    const bool fast = kscan_chain_process(dev);
    const int32_t period = fast ? config->debounce_scan_period_ms : config->poll_period_ms;

    // Skip missed periods instead of scanning back-to-back to catch up.
    const int64_t now = k_uptime_get();
    data->scan_time = MAX(data->scan_time + period, now);
    k_work_reschedule(&data->work, K_TIMEOUT_ABS_MS(data->scan_time));
}

/** Sets the callback. It cannot be cleared: kscan_disable_callback() stops the calls. */
static int kscan_chain_configure(const struct device *dev, const kscan_callback_t callback) {
    struct kscan_chain_data *data = dev->data;

    if (!callback) {
        return -EINVAL;
    }
    data->callback = callback;
    return 0;
}

static int kscan_chain_enable(const struct device *dev) {
    struct kscan_chain_data *data = dev->data;

    if (!data->callback) {
        return -EINVAL;
    }
    data->scan_time = k_uptime_get();
    k_work_reschedule(&data->work, K_NO_WAIT);
    return 0;
}

static int kscan_chain_disable(const struct device *dev) {
    struct kscan_chain_data *data = dev->data;

    k_work_cancel_delayable(&data->work);
    return 0;
}

static int kscan_chain_init(const struct device *dev) {
    const struct kscan_chain_config *config = dev->config;
    struct kscan_chain_data *data = dev->data;

    data->dev = dev;

    if (!spi_is_ready_dt(&config->bus)) {
        LOG_ERR("SPI bus %s is not ready", config->bus.bus->name);
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&config->load)) {
        LOG_ERR("GPIO %s for /PL is not ready", config->load.port->name);
        return -ENODEV;
    }

    const int err = gpio_pin_configure_dt(&config->load, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("Unable to configure /PL pin %u on %s: %d", config->load.pin,
                config->load.port->name, err);
        return err;
    }

    chain_init(&data->chain, &config->chain, data->reported);
    k_work_init_delayable(&data->work, kscan_chain_work_handler);
    return 0;
}

static const struct kscan_driver_api kscan_chain_api = {
    .config = kscan_chain_configure,
    .enable_callback = kscan_chain_enable,
    .disable_callback = kscan_chain_disable,
};

#define INST_KEYS(n) (DT_INST_PROP(n, rows) * DT_INST_PROP(n, columns))
#define INST_READ_LEN(n) (DT_INST_PROP(n, max_key_modules) + 1)

#define KSCAN_CHAIN_INIT(n)                                                                        \
    BUILD_ASSERT(DT_INST_PROP(n, rows) >= 1 && DT_INST_PROP(n, rows) <= CHAIN_MAX_ROWS,            \
                 "rows must be 1 to 7");                                                           \
    BUILD_ASSERT(DT_INST_PROP(n, columns) >= 1, "columns must be at least 1");                     \
    BUILD_ASSERT(DT_INST_PROP(n, max_key_modules) >= 1, "max-key-modules must be at least 1");     \
    BUILD_ASSERT(DT_INST_PROP(n, stable_scans) >= 1, "stable-scans must be at least 1");           \
    BUILD_ASSERT(DT_INST_PROP(n, debounce_press_ms) <= DEBOUNCE_COUNTER_MAX,                       \
                 "debounce-press-ms is too large");                                                \
    BUILD_ASSERT(DT_INST_PROP(n, debounce_release_ms) <= DEBOUNCE_COUNTER_MAX,                     \
                 "debounce-release-ms is too large");                                              \
                                                                                                   \
    static uint8_t kscan_chain_buf_##n[INST_READ_LEN(n)];                                          \
    static bool kscan_chain_active_##n[INST_KEYS(n)];                                              \
    static bool kscan_chain_pressed_##n[INST_KEYS(n)];                                             \
    static bool kscan_chain_reported_##n[INST_KEYS(n)];                                            \
    static struct zmk_debounce_state kscan_chain_debounce_##n[INST_KEYS(n)];                       \
                                                                                                   \
    static struct kscan_chain_data kscan_chain_data_##n = {                                        \
        .buf = kscan_chain_buf_##n,                                                                \
        .active = kscan_chain_active_##n,                                                          \
        .pressed = kscan_chain_pressed_##n,                                                        \
        .reported = kscan_chain_reported_##n,                                                      \
        .debounce = kscan_chain_debounce_##n,                                                      \
    };                                                                                             \
                                                                                                   \
    static const struct kscan_chain_config kscan_chain_config_##n = {                              \
        .bus = SPI_DT_SPEC_INST_GET(n, SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8),    \
                                    0),                                                            \
        .load = GPIO_DT_SPEC_INST_GET(n, load_gpios),                                              \
        .chain =                                                                                   \
            {                                                                                      \
                .anchor = (enum chain_anchor)DT_INST_ENUM_IDX(n, anchor),                          \
                .columns = DT_INST_PROP(n, columns),                                               \
                .rows = DT_INST_PROP(n, rows),                                                     \
                .stable_scans = DT_INST_PROP(n, stable_scans),                                     \
                .recovery_scans = DT_INST_PROP(n, recovery_scans),                                 \
            },                                                                                     \
        .debounce =                                                                                \
            {                                                                                      \
                .debounce_press_ms = DT_INST_PROP(n, debounce_press_ms),                           \
                .debounce_release_ms = DT_INST_PROP(n, debounce_release_ms),                       \
            },                                                                                     \
        .read_len = INST_READ_LEN(n),                                                              \
        .keys = INST_KEYS(n),                                                                      \
        .debounce_scan_period_ms = DT_INST_PROP(n, debounce_scan_period_ms),                       \
        .poll_period_ms = DT_INST_PROP(n, poll_period_ms),                                         \
    };                                                                                             \
                                                                                                   \
    DEVICE_DT_INST_DEFINE(n, kscan_chain_init, NULL, &kscan_chain_data_##n,                        \
                          &kscan_chain_config_##n, POST_KERNEL,                                    \
                          CONFIG_ROAMYBOARD_KSCAN_165_CHAIN_INIT_PRIORITY, &kscan_chain_api);

DT_INST_FOREACH_STATUS_OKAY(KSCAN_CHAIN_INIT)
