/*
 * CyberFly: Settings reset + normal reboot behavior.
 * Erases NVS settings (BLE bonds, device name cache, etc.) then reboots normally.
 * Used for clearing stale BLE pairing/name without entering bootloader.
 */

#define DT_DRV_COMPAT cyberfly_behavior_reset_settings_bootloader

#include <zephyr/device.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/logging/log.h>

#include <drivers/behavior.h>
#include <zmk/behavior.h>
#include <zmk/settings.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    LOG_INF("Settings reset triggered - erasing NVS and rebooting");

    int rc = zmk_settings_erase();
    if (rc) {
        LOG_ERR("Failed to erase settings: %d", rc);
    } else {
        LOG_INF("Settings erased successfully, rebooting...");
    }

    sys_reboot(SYS_REBOOT_WARM);

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
