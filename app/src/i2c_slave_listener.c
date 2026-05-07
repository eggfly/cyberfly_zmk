/*
 * ZMK event listener that drives the CyberFly I2C slave register file.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/modifiers_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/hid.h>

LOG_MODULE_DECLARE(cyberfly_i2c_slave, CONFIG_ZMK_LOG_LEVEL);

extern void cyberfly_i2c_slave_on_position(uint32_t position, bool pressed);
extern void cyberfly_i2c_slave_on_keycode(uint16_t usage_page, uint32_t keycode,
                                           uint8_t modifiers, bool pressed);
extern void cyberfly_i2c_slave_on_modifiers(uint8_t modifiers);
extern void cyberfly_i2c_slave_on_layer(uint8_t layer);
extern void cyberfly_i2c_slave_on_battery(uint8_t soc);
extern void cyberfly_i2c_slave_on_usb(uint8_t usb_state);

static int cyberfly_i2c_listener(const zmk_event_t *eh)
{
    const struct zmk_position_state_changed *pos_ev = as_zmk_position_state_changed(eh);
    if (pos_ev) {
        cyberfly_i2c_slave_on_position(pos_ev->position, pos_ev->state);
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_keycode_state_changed *kc_ev = as_zmk_keycode_state_changed(eh);
    if (kc_ev) {
        uint8_t mods = kc_ev->implicit_modifiers | kc_ev->explicit_modifiers |
                       zmk_hid_get_explicit_mods();
        cyberfly_i2c_slave_on_keycode(kc_ev->usage_page, kc_ev->keycode, mods, kc_ev->state);
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_modifiers_state_changed *mod_ev = as_zmk_modifiers_state_changed(eh);
    if (mod_ev) {
        cyberfly_i2c_slave_on_modifiers(zmk_hid_get_explicit_mods());
        return ZMK_EV_EVENT_BUBBLE;
    }

    const struct zmk_layer_state_changed *layer_ev = as_zmk_layer_state_changed(eh);
    if (layer_ev) {
        if (layer_ev->state) {
            cyberfly_i2c_slave_on_layer(layer_ev->layer);
        }
        return ZMK_EV_EVENT_BUBBLE;
    }

#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
    const struct zmk_battery_state_changed *bat_ev = as_zmk_battery_state_changed(eh);
    if (bat_ev) {
        cyberfly_i2c_slave_on_battery(bat_ev->state_of_charge);
        return ZMK_EV_EVENT_BUBBLE;
    }
#endif

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    const struct zmk_usb_conn_state_changed *usb_ev = as_zmk_usb_conn_state_changed(eh);
    if (usb_ev) {
        cyberfly_i2c_slave_on_usb((uint8_t)usb_ev->conn_state);
        return ZMK_EV_EVENT_BUBBLE;
    }
#endif

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(cyberfly_i2c_slave, cyberfly_i2c_listener);
ZMK_SUBSCRIPTION(cyberfly_i2c_slave, zmk_position_state_changed);
ZMK_SUBSCRIPTION(cyberfly_i2c_slave, zmk_keycode_state_changed);
ZMK_SUBSCRIPTION(cyberfly_i2c_slave, zmk_modifiers_state_changed);
ZMK_SUBSCRIPTION(cyberfly_i2c_slave, zmk_layer_state_changed);
#if IS_ENABLED(CONFIG_ZMK_BATTERY_REPORTING)
ZMK_SUBSCRIPTION(cyberfly_i2c_slave, zmk_battery_state_changed);
#endif
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(cyberfly_i2c_slave, zmk_usb_conn_state_changed);
#endif
