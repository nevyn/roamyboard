/*
 * The BLE-MIDI GATT service, next to ZMK's HID service on the same host connection.
 * docs/midi.md describes it.
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <string.h>

#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/ble.h>

#include <roamyboard/midi.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static const struct bt_uuid_128 midi_service_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x03B80E5A, 0xEDE8, 0x4B33, 0xA751, 0x6CE34EC4C700));
static const struct bt_uuid_128 midi_io_uuid = BT_UUID_INIT_128(
    BT_UUID_128_ENCODE(0x7772E5DB, 0x3868, 0x4112, 0xA1A9, 0xF2669D106BF3));

// The BLE-MIDI spec requires a read to return no payload.
static ssize_t read_io(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                       uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, NULL, 0);
}

static ssize_t write_io(struct bt_conn *conn, const struct bt_gatt_attr *attr, const void *buf,
                        uint16_t len, uint16_t offset, uint8_t flags) {
    LOG_DBG("Ignoring %u bytes of MIDI from the host", len);
    return len;
}

static void io_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    LOG_INF("Host %s MIDI notifications", value & BT_GATT_CCC_NOTIFY ? "enabled" : "disabled");
}

BT_GATT_SERVICE_DEFINE(midi_svc, BT_GATT_PRIMARY_SERVICE(&midi_service_uuid),
                       BT_GATT_CHARACTERISTIC(&midi_io_uuid.uuid,
                                              BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE_WITHOUT_RESP |
                                                  BT_GATT_CHRC_NOTIFY,
                                              BT_GATT_PERM_READ_ENCRYPT |
                                                  BT_GATT_PERM_WRITE_ENCRYPT,
                                              read_io, write_io, NULL),
                       BT_GATT_CCC(io_ccc_changed,
                                   BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT));

#define IO_ATTR (&midi_svc.attrs[2])

/** One BLE-MIDI packet with one message: header, timestamp, then the message. */
struct packet {
    uint8_t len;
    uint8_t bytes[2 + ROAMYBOARD_MIDI_MSG_MAX];
};

K_MSGQ_DEFINE(midi_msgq, sizeof(struct packet), 16, 1);

// Its own queue, as ZMK's HOG has: on the system work queue, a notify cannot wait for a buffer.
K_THREAD_STACK_DEFINE(midi_q_stack, CONFIG_ZMK_BLE_THREAD_STACK_SIZE);
static struct k_work_q midi_work_q;

static void send_packets(struct k_work *work) {
    struct packet packet;
    while (k_msgq_get(&midi_msgq, &packet, K_NO_WAIT) == 0) {
        struct bt_conn *conn = zmk_ble_active_profile_conn();
        if (conn == NULL) {
            continue;
        }
        if (!bt_gatt_is_subscribed(conn, IO_ATTR, BT_GATT_CCC_NOTIFY)) {
            LOG_DBG("Host has not subscribed to MIDI; dropping %02x", packet.bytes[2]);
            bt_conn_unref(conn);
            continue;
        }
        const int err = bt_gatt_notify(conn, IO_ATTR, packet.bytes, packet.len);
        if (err == -EPERM) {
            bt_conn_set_security(conn, BT_SECURITY_L2);
        } else if (err) {
            LOG_WRN("MIDI notify of %02x failed: %d", packet.bytes[2], err);
        }
        bt_conn_unref(conn);
    }
}

static K_WORK_DEFINE(send_work, send_packets);

int roamyboard_midi_send(const uint8_t *msg, size_t len, int64_t timestamp_ms) {
    if (len < 1 || len > ROAMYBOARD_MIDI_MSG_MAX || !(msg[0] & 0x80)) {
        return -EINVAL;
    }
    const uint16_t ts = (uint16_t)(timestamp_ms & 0x1FFF);
    struct packet packet = {.len = (uint8_t)(2 + len)};
    packet.bytes[0] = 0x80 | (ts >> 7);
    packet.bytes[1] = 0x80 | (ts & 0x7F);
    memcpy(&packet.bytes[2], msg, len);

    const int err = k_msgq_put(&midi_msgq, &packet, K_MSEC(100));
    if (err) {
        LOG_WRN("MIDI queue full; dropping %02x", msg[0]);
        return -ENOMEM;
    }
    k_work_submit_to_queue(&midi_work_q, &send_work);
    return 0;
}

static int midi_init(void) {
    k_work_queue_start(&midi_work_q, midi_q_stack, K_THREAD_STACK_SIZEOF(midi_q_stack),
                       CONFIG_ZMK_BLE_THREAD_PRIORITY, NULL);
    return 0;
}

SYS_INIT(midi_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
