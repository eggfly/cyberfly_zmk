/*
 * CyberFly Battery Debug BLE Service.
 *
 * Custom GATT service exposing raw VDDH millivolts and SoC percentage.
 * Use LightBlue / nRF Connect to read or subscribe to notifications.
 *
 * Service UUID:  0xCF01 (under Bluetooth SIG base)
 *   Actually using full 128-bit: 0000cf01-0000-1000-8000-00805f9b34fb
 *
 * Characteristics:
 *   0xCF02 — Battery mV    (uint16 LE, read + notify)
 *   0xCF03 — Battery SoC % (uint8, read + notify)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define BATT_DBG_SVC_UUID BT_UUID_DECLARE_16(0xCF01)
#define BATT_DBG_MV_UUID  BT_UUID_DECLARE_16(0xCF02)
#define BATT_DBG_SOC_UUID BT_UUID_DECLARE_16(0xCF03)

static uint16_t batt_mv;
static uint8_t batt_soc;

#if DT_HAS_CHOSEN(zmk_battery)
static const struct device *const batt_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_battery));
#endif

static ssize_t read_mv(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                       void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &batt_mv, sizeof(batt_mv));
}

static ssize_t read_soc(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                        void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &batt_soc, sizeof(batt_soc));
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
        BT_GATT_PERM_READ, read_mv, NULL, &batt_mv),
    BT_GATT_CCC(mv_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(BATT_DBG_SOC_UUID,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ, read_soc, NULL, &batt_soc),
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

    batt_mv = voltage.val1 * 1000 + voltage.val2 / 1000;
    batt_soc = soc.val1;

    LOG_INF("Battery: %d mV, %d%%", batt_mv, batt_soc);

    if (mv_notify_enabled) {
        bt_gatt_notify(NULL, &batt_debug_svc.attrs[2], &batt_mv, sizeof(batt_mv));
    }
    if (soc_notify_enabled) {
        bt_gatt_notify(NULL, &batt_debug_svc.attrs[5], &batt_soc, sizeof(batt_soc));
    }
#endif
}

static void batt_debug_work_handler(struct k_work *work) {
    batt_debug_sample();
    k_work_reschedule((struct k_work_delayable *)work, K_SECONDS(10));
}

static K_WORK_DELAYABLE_DEFINE(batt_debug_work, batt_debug_work_handler);

static int batt_debug_init(void) {
    LOG_INF("Battery debug BLE service started (CF01/CF02/CF03)");
    k_work_reschedule(&batt_debug_work, K_SECONDS(3));
    return 0;
}

SYS_INIT(batt_debug_init, APPLICATION, 99);
