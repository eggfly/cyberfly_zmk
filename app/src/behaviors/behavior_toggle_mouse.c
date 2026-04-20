/*
 * CyberFly: Toggle mouse on/off behavior.
 * Fn+RShift combo toggles a global flag; mouse_test.c reads it.
 */

#define DT_DRV_COMPAT cyberfly_behavior_toggle_mouse

#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static bool mouse_enabled = true;

bool cyberfly_mouse_is_enabled(void) {
    return mouse_enabled;
}

extern void cyberfly_rgb_flash_mouse_toggle(void);

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    mouse_enabled = !mouse_enabled;
    LOG_INF("Mouse %s", mouse_enabled ? "enabled" : "disabled");
    cyberfly_rgb_flash_mouse_toggle();
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
