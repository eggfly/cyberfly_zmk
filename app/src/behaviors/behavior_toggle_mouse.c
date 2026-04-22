/*
 * CyberFly: Cycle mouse mode OFF → M1 (Kalman+SS) → M2 (accel) → OFF.
 */

#define DT_DRV_COMPAT cyberfly_behavior_toggle_mouse

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <cyberfly/mouse_mode.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static enum cyberfly_mouse_mode mouse_mode = CYBERFLY_MOUSE_OFF;

enum cyberfly_mouse_mode cyberfly_mouse_get_mode(void) {
    return mouse_mode;
}

bool cyberfly_mouse_is_enabled(void) {
    return mouse_mode != CYBERFLY_MOUSE_OFF;
}

extern void cyberfly_rgb_flash_mouse_mode(enum cyberfly_mouse_mode mode);

static const char *mode_name(enum cyberfly_mouse_mode m) {
    switch (m) {
    case CYBERFLY_MOUSE_OFF: return "OFF";
    case CYBERFLY_MOUSE_M1:  return "M1-kalman";
    case CYBERFLY_MOUSE_M2:  return "M2-accel";
    default:                 return "?";
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    mouse_mode = (mouse_mode + 1) % CYBERFLY_MOUSE_MODE_COUNT;
    LOG_INF("Mouse mode: %s", mode_name(mouse_mode));
    cyberfly_rgb_flash_mouse_mode(mouse_mode);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define INST(n)                                                                \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,            \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_api);

DT_INST_FOREACH_STATUS_OKAY(INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
