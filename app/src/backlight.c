/*
 * Copyright (c) 2021 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>

#include <zmk/activity.h>
#include <zmk/backlight.h>
#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

BUILD_ASSERT(DT_HAS_CHOSEN(zmk_backlight),
             "CONFIG_ZMK_BACKLIGHT is enabled but no zmk,backlight chosen node found");

static const struct device *const backlight_dev = DEVICE_DT_GET(DT_CHOSEN(zmk_backlight));

#define CHILD_COUNT(...) +1
#define DT_NUM_CHILD(node_id) (DT_FOREACH_CHILD(node_id, CHILD_COUNT))

#define BACKLIGHT_NUM_LEDS (DT_NUM_CHILD(DT_CHOSEN(zmk_backlight)))

#define BRT_MAX 100

/* PWM duty correction for AP3032 boost driver at 25kHz.
 * Dead zone: 0-2% PWM produces no visible light.
 * Saturation: above ~30% brightness barely changes.
 * Maps user 0-100% to PWM 0,3-30% (gamma 2.0, skipping dead zone). */
static const uint8_t gamma_lut[101] = {
      0,  3,  3,  3,  3,  3,  3,  3,  3,  3,
      3,  3,  3,  3,  4,  4,  4,  4,  4,  4,
      4,  4,  4,  4,  5,  5,  5,  5,  5,  5,
      5,  6,  6,  6,  6,  6,  6,  7,  7,  7,
      7,  8,  8,  8,  8,  8,  9,  9,  9,  9,
     10, 10, 10, 11, 11, 11, 11, 12, 12, 12,
     13, 13, 13, 14, 14, 14, 15, 15, 15, 16,
     16, 17, 17, 17, 18, 18, 19, 19, 19, 20,
     20, 21, 21, 22, 22, 23, 23, 23, 24, 24,
     25, 25, 26, 26, 27, 27, 28, 28, 29, 29,
     30
};

struct backlight_state {
    uint8_t brightness;
    bool on;
};

static struct backlight_state state = {.brightness = CONFIG_ZMK_BACKLIGHT_BRT_START,
                                       .on = IS_ENABLED(CONFIG_ZMK_BACKLIGHT_ON_START)};

static int zmk_backlight_update(void) {
    uint8_t brt = zmk_backlight_get_brt();
#if IS_ENABLED(CONFIG_ZMK_USB)
    if (!zmk_usb_is_powered()) {
        brt = 0;
    }
#endif
    uint8_t pwm_brt = gamma_lut[MIN(brt, 100)];
    LOG_DBG("Update backlight brightness: %d%% -> PWM %d%%", brt, pwm_brt);

    for (int i = 0; i < BACKLIGHT_NUM_LEDS; i++) {
        int rc = led_set_brightness(backlight_dev, i, pwm_brt);
        if (rc != 0) {
            LOG_ERR("Failed to update backlight LED %d: %d", i, rc);
            return rc;
        }
    }
    return 0;
}

#if IS_ENABLED(CONFIG_SETTINGS)
static int backlight_settings_load_cb(const char *name, size_t len, settings_read_cb read_cb,
                                      void *cb_arg) {
    const char *next;
    if (settings_name_steq(name, "state", &next) && !next) {
        if (len != sizeof(state)) {
            return -EINVAL;
        }

        int rc = read_cb(cb_arg, &state, sizeof(state));
        if (rc >= 0) {
            rc = zmk_backlight_update();
        }

        return MIN(rc, 0);
    }
    return -ENOENT;
}

SETTINGS_STATIC_HANDLER_DEFINE(backlight, "backlight", NULL, backlight_settings_load_cb, NULL,
                               NULL);

static void backlight_save_work_handler(struct k_work *work) {
    settings_save_one("backlight/state", &state, sizeof(state));
}

static struct k_work_delayable backlight_save_work;
#endif

static int zmk_backlight_init(void) {
    if (!device_is_ready(backlight_dev)) {
        LOG_ERR("Backlight device \"%s\" is not ready", backlight_dev->name);
        return -ENODEV;
    }

#if IS_ENABLED(CONFIG_SETTINGS)
    k_work_init_delayable(&backlight_save_work, backlight_save_work_handler);
#endif
#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_USB)
    if (zmk_usb_is_powered()) {
        state.brightness = CONFIG_ZMK_BACKLIGHT_BRT_STEP;
        state.on = true;
    } else {
        state.on = false;
    }
#endif
    return zmk_backlight_update();
}

static int zmk_backlight_update_and_save(void) {
    int rc = zmk_backlight_update();
    if (rc != 0) {
        return rc;
    }

#if IS_ENABLED(CONFIG_SETTINGS)
    int ret = k_work_reschedule(&backlight_save_work, K_MSEC(CONFIG_ZMK_SETTINGS_SAVE_DEBOUNCE));
    return MIN(ret, 0);
#else
    return 0;
#endif
}

int zmk_backlight_on(void) {
    state.brightness = MAX(state.brightness, CONFIG_ZMK_BACKLIGHT_BRT_STEP);
    state.on = true;
    return zmk_backlight_update_and_save();
}

int zmk_backlight_off(void) {
    state.on = false;
    return zmk_backlight_update_and_save();
}

int zmk_backlight_toggle(void) { return state.on ? zmk_backlight_off() : zmk_backlight_on(); }

bool zmk_backlight_is_on(void) { return state.on; }

int zmk_backlight_set_brt(uint8_t brightness) {
    state.brightness = MIN(brightness, BRT_MAX);
    state.on = (state.brightness > 0);
    return zmk_backlight_update_and_save();
}

uint8_t zmk_backlight_get_brt(void) { return state.on ? state.brightness : 0; }

uint8_t zmk_backlight_calc_brt(int direction) {
    int brt = state.brightness + (direction * CONFIG_ZMK_BACKLIGHT_BRT_STEP);
    return CLAMP(brt, 0, BRT_MAX);
}

uint8_t zmk_backlight_calc_brt_cycle(void) {
    if (state.brightness == BRT_MAX) {
        return 0;
    } else {
        return zmk_backlight_calc_brt(1);
    }
}

#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_IDLE) || IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_USB)
static int backlight_auto_state(bool *prev_state, bool new_state) {
    if (state.on == new_state) {
        return 0;
    }
    state.on = new_state && *prev_state;
    *prev_state = !new_state;
    return zmk_backlight_update();
}

static int backlight_event_listener(const zmk_event_t *eh) {

#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_IDLE)
    if (as_zmk_activity_state_changed(eh)) {
        static bool prev_state = false;
        return backlight_auto_state(&prev_state, zmk_activity_get_state() == ZMK_ACTIVITY_ACTIVE);
    }
#endif

#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_USB)
    if (as_zmk_usb_conn_state_changed(eh)) {
        if (zmk_usb_is_powered()) {
            state.brightness = CONFIG_ZMK_BACKLIGHT_BRT_STEP;
            state.on = true;
        } else {
            state.on = false;
        }
        return zmk_backlight_update();
    }
#endif

    return -ENOTSUP;
}

ZMK_LISTENER(backlight, backlight_event_listener);
#endif // IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_IDLE) ||
       // IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_USB)

#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_IDLE)
ZMK_SUBSCRIPTION(backlight, zmk_activity_state_changed);
#endif

#if IS_ENABLED(CONFIG_ZMK_BACKLIGHT_AUTO_OFF_USB)
ZMK_SUBSCRIPTION(backlight, zmk_usb_conn_state_changed);
#endif

SYS_INIT(zmk_backlight_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
