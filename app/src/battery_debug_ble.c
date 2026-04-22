/*
 * CyberFly Battery Debug BLE Service.
 *
 * Custom GATT service exposing battery info as UTF-8 strings
 * so LightBlue / nRF Connect display them as readable text.
 *
 * Service UUID: 0xCF01
 *   0xCF02 — Battery mV    (UTF-8 string, e.g. "3825 mV")
 *   0xCF03 — Battery SoC   (UTF-8 string, e.g. "57%")
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <stdio.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define BATT_DBG_SVC_UUID BT_UUID_DECLARE_16(0xCF01)
#define BATT_DBG_MV_UUID  BT_UUID_DECLARE_16(0xCF02)
#define BATT_DBG_SOC_UUID BT_UUID_DECLARE_16(0xCF03)

static char mv_str[16];
static char soc_str[16];

#if DT_HAS_CHOSEN(zmk_battery)
static const struct device *const batt_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
#endif

static ssize_t read_mv(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                       void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             mv_str, strlen(mv_str));
}

static ssize_t read_soc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset,
                             soc_str, strlen(soc_str));
}

static bool mv_notify_enabled;
static bool soc_notify_enabled;

static void mv_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    mv_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

static void soc_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {
    soc_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
}

BT_GATT_SERVICE_DEFINE(batt_debug_svc,
    BT_GATT_PRIMARY_SERVICE(BATT_DBG_SVC_UUID),

    BT_GATT_CHARACTERISTIC(BATT_DBG_MV_UUID,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ, read_mv, NULL, NULL),
    BT_GATT_CCC(mv_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BATT_DBG_SOC_UUID,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ, read_soc, NULL, NULL),
    BT_GATT_CCC(soc_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static void batt_debug_sample(void) {
#if DT_HAS_CHOSEN(zmk_battery)
    if (!device_is_ready(batt_dev)) {
        return;
    }

    int rc = sensor_sample_fetch(batt_dev);
    if (rc) {
        LOG_ERR("Battery fetch failed: %d", rc);
        return;
    }

    struct sensor_value voltage, soc;
    sensor_channel_get(batt_dev, SENSOR_CHAN_GAUGE_VOLTAGE, &voltage);
    sensor_channel_get(batt_dev, SENSOR_CHAN_GAUGE_STATE_OF_CHARGE, &soc);

    uint16_t batt_mv = voltage.val1 * 1000 + voltage.val2 / 1000;
    uint8_t batt_soc = soc.val1;

    snprintf(mv_str, sizeof(mv_str), "%u mV", batt_mv);
    snprintf(soc_str, sizeof(soc_str), "%u%%", batt_soc);

    LOG_INF("Battery: %s, %s", mv_str, soc_str);

    if (mv_notify_enabled) {
        bt_gatt_notify(NULL, &batt_debug_svc.attrs[2],
                       mv_str, strlen(mv_str));
    }
    if (soc_notify_enabled) {
        bt_gatt_notify(NULL, &batt_debug_svc.attrs[5],
                       soc_str, strlen(soc_str));
    }
#endif
}

static void batt_debug_work_handler(struct k_work *work) {
    batt_debug_sample();
    k_work_reschedule((struct k_work_delayable *)work, K_SECONDS(10));
}

static K_WORK_DELAYABLE_DEFINE(batt_debug_work, batt_debug_work_handler);

static int batt_debug_init(void) {
    snprintf(mv_str, sizeof(mv_str), "-- mV");
    snprintf(soc_str, sizeof(soc_str), "--%%" );
    LOG_INF("Battery debug BLE service started (CF01/CF02/CF03)");
    k_work_reschedule(&batt_debug_work, K_SECONDS(3));
    return 0;
}

SYS_INIT(batt_debug_init, APPLICATION, 99);
