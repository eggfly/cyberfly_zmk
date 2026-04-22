/*
 * CyberFly Air Mouse — QMI8658A 6-axis IMU.
 * M1: Gyro rate → cursor, minimal processing
 * M2: Accelerometer tilt-to-velocity (fallback)
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include <math.h>

#include <zmk/hid.h>
#include <zmk/endpoints.h>
#include <cyberfly/mouse_mode.h>

LOG_MODULE_REGISTER(air_mouse, LOG_LEVEL_INF);

#define IMU_NODE DT_NODELABEL(qmi8658a)
static const struct device *const imu = DEVICE_DT_GET(IMU_NODE);
extern enum cyberfly_mouse_mode cyberfly_mouse_get_mode(void);

#define POLL_INTERVAL_MS  18
#define RAD_TO_DPS        (180.0f / 3.14159265f)

/* M1: calibration */
#define CAL_DISCARD       10
#define CAL_COLLECT       50

/* M1: cursor mapping — intentionally simple */
#define DEADZONE          1.5f
#define SENSITIVITY       0.4f
#define MAX_OUT           127

/* M2: accel tilt */
#define ACCEL_DEAD_ZONE   0.15f
#define ACCEL_SENSITIVITY 3.0f
#define ACCEL_MAX_SPEED   15

static bool imu_ok;
static uint32_t poll_count;

/* M1 state */
enum { CAL_IDLE, CAL_DISCARDING, CAL_COLLECTING, CAL_READY };
static int  cal_phase;
static int  cal_cnt;
static float cal_sum[3];
static float bias[3];
static bool  m1_active;

static void m1_reset(void) {
    cal_phase = CAL_IDLE;
    cal_cnt = 0;
    cal_sum[0] = cal_sum[1] = cal_sum[2] = 0.0f;
    bias[0] = bias[1] = bias[2] = 0.0f;
    m1_active = false;
}

static bool m1_cal_tick(float gx, float gy, float gz) {
    switch (cal_phase) {
    case CAL_IDLE:
        cal_phase = CAL_DISCARDING;
        cal_cnt = 0;
        LOG_INF("M1 calibrating ~1s...");
        return false;
    case CAL_DISCARDING:
        if (++cal_cnt >= CAL_DISCARD) {
            cal_phase = CAL_COLLECTING;
            cal_cnt = 0;
            cal_sum[0] = cal_sum[1] = cal_sum[2] = 0.0f;
        }
        return false;
    case CAL_COLLECTING:
        cal_sum[0] += gx;
        cal_sum[1] += gy;
        cal_sum[2] += gz;
        if (++cal_cnt >= CAL_COLLECT) {
            bias[0] = cal_sum[0] / CAL_COLLECT;
            bias[1] = cal_sum[1] / CAL_COLLECT;
            bias[2] = cal_sum[2] / CAL_COLLECT;
            cal_phase = CAL_READY;
            m1_active = true;
            LOG_INF("M1 ready: bias gx=%.2f gy=%.2f gz=%.2f",
                    (double)bias[0], (double)bias[1], (double)bias[2]);
        }
        return false;
    case CAL_READY:
        return true;
    }
    return false;
}

static int16_t rate_to_cursor(float dps) {
    if (dps > -DEADZONE && dps < DEADZONE)
        return 0;
    float out = dps * SENSITIVITY;
    if (out > MAX_OUT)  out = MAX_OUT;
    if (out < -MAX_OUT) out = -MAX_OUT;
    return (int16_t)out;
}

static int16_t accel_to_mouse(float val) {
    if (val > -ACCEL_DEAD_ZONE && val < ACCEL_DEAD_ZONE)
        return 0;
    int16_t r = (int16_t)(val * ACCEL_SENSITIVITY);
    if (r > ACCEL_MAX_SPEED)  r = ACCEL_MAX_SPEED;
    if (r < -ACCEL_MAX_SPEED) r = -ACCEL_MAX_SPEED;
    return r;
}

/* ── Work handler ───────────────────────────────── */
static void air_mouse_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(air_mouse_work, air_mouse_work_handler);

static void air_mouse_work_handler(struct k_work *work) {
    if (!imu_ok) {
        if (!device_is_ready(imu)) {
            LOG_ERR("QMI8658A not ready (retry in 5s)");
            k_work_reschedule(&air_mouse_work, K_SECONDS(5));
            return;
        }
        int rc = sensor_sample_fetch(imu);
        if (rc) {
            LOG_ERR("QMI8658A first fetch failed: %d (retry in 5s)", rc);
            k_work_reschedule(&air_mouse_work, K_SECONDS(5));
            return;
        }
        imu_ok = true;
        LOG_INF("QMI8658A online");
    }

    k_work_reschedule(&air_mouse_work, K_MSEC(POLL_INTERVAL_MS));

    enum cyberfly_mouse_mode mode = cyberfly_mouse_get_mode();
    if (mode == CYBERFLY_MOUSE_OFF) {
        if (m1_active || cal_phase != CAL_IDLE)
            m1_reset();
        return;
    }

    int rc = sensor_sample_fetch(imu);
    if (rc) {
        if ((poll_count % 500) == 0)
            LOG_ERR("fetch fail: %d cnt=%u", rc, poll_count);
        poll_count++;
        return;
    }

    int16_t dx = 0, dy = 0;

    if (mode == CYBERFLY_MOUSE_M1) {
        struct sensor_value gyro[3];
        sensor_channel_get(imu, SENSOR_CHAN_GYRO_XYZ, gyro);
        float gx = (float)sensor_value_to_double(&gyro[0]) * RAD_TO_DPS;
        float gy = (float)sensor_value_to_double(&gyro[1]) * RAD_TO_DPS;
        float gz = (float)sensor_value_to_double(&gyro[2]) * RAD_TO_DPS;

        if (!m1_cal_tick(gx, gy, gz)) {
            poll_count++;
            return;
        }

        float rx = gx - bias[0];
        float ry = gy - bias[1];
        float rz = gz - bias[2];

        if ((poll_count % 100) == 0) {
            LOG_INF("gyro rx=%.1f ry=%.1f rz=%.1f",
                    (double)rx, (double)ry, (double)rz);
        }

        /* Axis mapping — chip mounting: rz=yaw→dx, rx=pitch→dy */
        dx = rate_to_cursor(-rz);
        dy = rate_to_cursor(-rx);

    } else {
        if (m1_active || cal_phase != CAL_IDLE)
            m1_reset();
        struct sensor_value accel[3];
        sensor_channel_get(imu, SENSOR_CHAN_ACCEL_XYZ, accel);
        float ax = (float)sensor_value_to_double(&accel[0]);
        float ay = (float)sensor_value_to_double(&accel[1]);
        dx = accel_to_mouse(-ax);
        dy = accel_to_mouse(ay);
    }

    poll_count++;

    if (dx == 0 && dy == 0)
        return;

    zmk_hid_mouse_movement_set(dx, dy);
    zmk_endpoint_send_mouse_report();
    zmk_hid_mouse_movement_set(0, 0);
}

static int air_mouse_init(void) {
    LOG_INF("air_mouse init, probe in 3s");
    m1_reset();
    k_work_reschedule(&air_mouse_work, K_SECONDS(3));
    return 0;
}

SYS_INIT(air_mouse_init, APPLICATION, 99);
