/*
 * RGB Status LED for CyberFly keyboard.
 *
 * Boot: R->G->B sequential flash then off.
 * USB connected: continuous RGB cycle.
 * Battery only: brief blue double-blink every 5 seconds.
 * Mouse layer on: green double-blink then resume previous mode.
 *
 * PWM1 channels: 0=Red(P0.14), 1=Green(P0.16), 2=Blue(P0.19)
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zmk/usb.h>
#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define RGB_STATUS_NODE DT_NODELABEL(rgb_status_leds)

BUILD_ASSERT(DT_NODE_EXISTS(RGB_STATUS_NODE),
             "rgb_status_leds node not found in device tree");

static const struct device *const rgb_dev = DEVICE_DT_GET(RGB_STATUS_NODE);

#define LED_RED   0
#define LED_GREEN 1
#define LED_BLUE  2

/* Timing constants */
#define BOOT_FLASH_ON_MS     80
#define BOOT_FLASH_OFF_MS    40
#define USB_CYCLE_ON_MS      150
#define USB_CYCLE_OFF_MS     80
#define BATTERY_INTERVAL_MS  5000
#define BATTERY_FLASH_MS     30
#define BATTERY_GAP_MS       60
#define MOUSE_FLASH_MS       40
#define MOUSE_GAP_MS         80

#define RGB_BRT 1

/* State */
enum rgb_mode {
    RGB_MODE_BOOT,
    RGB_MODE_USB_CYCLE,
    RGB_MODE_BATTERY_PULSE,
    RGB_MODE_MOUSE_FLASH,
};

static enum rgb_mode current_mode = RGB_MODE_BOOT;
static uint8_t cycle_phase = 0;
static bool usb_connected = false;

/* Work items */
static void rgb_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(rgb_work, rgb_work_handler);

static int rgb_set(uint8_t r, uint8_t g, uint8_t b) {
    led_set_brightness(rgb_dev, LED_RED, r);
    led_set_brightness(rgb_dev, LED_GREEN, g);
    led_set_brightness(rgb_dev, LED_BLUE, b);
    return 0;
}

static void rgb_off(void) {
    rgb_set(0, 0, 0);
}

static void start_usb_cycle(void) {
    current_mode = RGB_MODE_USB_CYCLE;
    cycle_phase = 0;
    k_work_reschedule(&rgb_work, K_NO_WAIT);
}

static void start_battery_pulse(void) {
    current_mode = RGB_MODE_BATTERY_PULSE;
    cycle_phase = 0;
    rgb_off();
    k_work_reschedule(&rgb_work, K_MSEC(BATTERY_INTERVAL_MS));
}

static void resume_normal_mode(void) {
    if (usb_connected) {
        start_usb_cycle();
    } else {
        start_battery_pulse();
    }
}

static void rgb_work_handler(struct k_work *work) {
    switch (current_mode) {
    case RGB_MODE_BOOT:
        switch (cycle_phase) {
        case 0: rgb_set(RGB_BRT, 0, 0); break;
        case 1: rgb_off(); break;
        case 2: rgb_set(0, RGB_BRT, 0); break;
        case 3: rgb_off(); break;
        case 4: rgb_set(0, 0, RGB_BRT); break;
        case 5: rgb_off(); break;
        case 6:
            resume_normal_mode();
            return;
        }
        cycle_phase++;
        k_work_reschedule(&rgb_work,
            K_MSEC((cycle_phase % 2 == 1) ? BOOT_FLASH_ON_MS : BOOT_FLASH_OFF_MS));
        break;

    case RGB_MODE_USB_CYCLE:
        switch (cycle_phase % 6) {
        case 0: rgb_set(RGB_BRT, 0, 0); break;
        case 1: rgb_off(); break;
        case 2: rgb_set(0, RGB_BRT, 0); break;
        case 3: rgb_off(); break;
        case 4: rgb_set(0, 0, RGB_BRT); break;
        case 5: rgb_off(); break;
        }
        cycle_phase++;
        k_work_reschedule(&rgb_work,
            K_MSEC((cycle_phase % 2 == 1) ? USB_CYCLE_ON_MS : USB_CYCLE_OFF_MS));
        break;

    case RGB_MODE_BATTERY_PULSE:
        switch (cycle_phase) {
        case 0: rgb_set(0, 0, RGB_BRT); break;
        case 1: rgb_off(); break;
        case 2: rgb_set(0, 0, RGB_BRT); break;
        case 3: rgb_off(); break;
        }
        cycle_phase++;
        if (cycle_phase <= 3) {
            k_work_reschedule(&rgb_work,
                K_MSEC((cycle_phase % 2 == 1) ? BATTERY_FLASH_MS : BATTERY_GAP_MS));
        } else {
            cycle_phase = 0;
            k_work_reschedule(&rgb_work, K_MSEC(BATTERY_INTERVAL_MS));
        }
        break;

    case RGB_MODE_MOUSE_FLASH:
        switch (cycle_phase) {
        case 0: rgb_set(0, RGB_BRT, 0); break;
        case 1: rgb_off(); break;
        case 2: rgb_set(0, RGB_BRT, 0); break;
        case 3: rgb_off(); break;
        case 4:
            resume_normal_mode();
            return;
        }
        cycle_phase++;
        k_work_reschedule(&rgb_work,
            K_MSEC((cycle_phase % 2 == 1) ? MOUSE_FLASH_MS : MOUSE_GAP_MS));
        break;
    }
}

/* Event handler */
static int rgb_status_event_listener(const zmk_event_t *eh) {
    struct zmk_usb_conn_state_changed *usb_ev = as_zmk_usb_conn_state_changed(eh);
    if (usb_ev) {
        bool was_connected = usb_connected;
        usb_connected = (usb_ev->conn_state == ZMK_USB_CONN_HID);

        if (current_mode == RGB_MODE_BOOT) {
            return ZMK_EV_EVENT_BUBBLE;
        }

        if (usb_connected && !was_connected) {
            start_usb_cycle();
        } else if (!usb_connected && was_connected) {
            start_battery_pulse();
        }
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgb_status_led, rgb_status_event_listener);
ZMK_SUBSCRIPTION(rgb_status_led, zmk_usb_conn_state_changed);

void cyberfly_rgb_flash_mouse_toggle(void) {
    if (current_mode != RGB_MODE_BOOT) {
        current_mode = RGB_MODE_MOUSE_FLASH;
        cycle_phase = 0;
        k_work_reschedule(&rgb_work, K_NO_WAIT);
    }
}

/* Initialization */
static int rgb_status_led_init(void) {
    if (!device_is_ready(rgb_dev)) {
        LOG_ERR("RGB status LED device not ready");
        return -ENODEV;
    }

    LOG_INF("RGB status LED init");

    usb_connected = (zmk_usb_get_conn_state() == ZMK_USB_CONN_HID);

    current_mode = RGB_MODE_BOOT;
    cycle_phase = 0;
    k_work_reschedule(&rgb_work, K_NO_WAIT);

    return 0;
}

SYS_INIT(rgb_status_led_init, APPLICATION, 99);
