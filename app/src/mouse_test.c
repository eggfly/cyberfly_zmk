/*
 * Fake mouse input for BLE HID mouse testing.
 * Always on by default. Fn+RShift combo toggles on/off via
 * cyberfly_mouse_is_enabled() from behavior_toggle_mouse.c.
 * Enable with CONFIG_CYBERFLY_MOUSE_TEST=y, disable for production.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/hid.h>
#include <zmk/endpoints.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

extern bool cyberfly_mouse_is_enabled(void);

#define CIRCLE_STEPS 16
#define REPORT_INTERVAL_MS 30

static const int8_t circle_x[CIRCLE_STEPS] = {
    3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1, 0, 1, 2, 3};
static const int8_t circle_y[CIRCLE_STEPS] = {
    0, 1, 2, 3, 3, 3, 2, 1, 0, -1, -2, -3, -3, -3, -2, -1};

static uint8_t step = 0;

static void mouse_test_work_handler(struct k_work *work) {
    if (cyberfly_mouse_is_enabled()) {
        zmk_hid_mouse_movement_set(circle_x[step], circle_y[step]);
        zmk_endpoint_send_mouse_report();
        zmk_hid_mouse_clear();
        step = (step + 1) % CIRCLE_STEPS;
    }

    k_work_reschedule((struct k_work_delayable *)work, K_MSEC(REPORT_INTERVAL_MS));
}

static K_WORK_DELAYABLE_DEFINE(mouse_test_work, mouse_test_work_handler);

static int mouse_test_init(void) {
    LOG_INF("Mouse test: Fn+RShift to toggle on/off");
    k_work_reschedule(&mouse_test_work, K_MSEC(2000));
    return 0;
}

SYS_INIT(mouse_test_init, APPLICATION, 99);
