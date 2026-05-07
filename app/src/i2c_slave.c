/*
 * CyberFly I2C slave target.
 *
 * Exposes the keyboard as an I2C slave device with a register-file + FIFO
 * protocol that is a superset of M5 CardKB (bitwise firmware). See
 * cyberfly-i2c-slave-protocol.md in the repo root for the full spec.
 *
 * Wire format:
 *   - Master write with N>=1 bytes: first byte = register address, remaining
 *     bytes are the write payload.
 *   - Master write with 0 bytes followed by read: sets the auto-advance read
 *     pointer to the last addressed register (or to REG_FIFO_ASCII if none).
 *   - Master read without preceding write: pops REG_FIFO_ASCII (CardKB2
 *     compat mode).
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <string.h>

LOG_MODULE_REGISTER(cyberfly_i2c_slave, CONFIG_ZMK_LOG_LEVEL);

extern uint8_t cyberfly_i2c_slave_keycode_to_ascii(uint16_t usage_page,
                                                    uint32_t keycode,
                                                    uint8_t modifiers);

#define I2C_BUS_NODE DT_ALIAS(cyberfly_i2c_slave_bus)

#if !DT_NODE_EXISTS(I2C_BUS_NODE)
#error "cyberfly-i2c-slave-bus alias not defined in device tree overlay"
#endif

static const struct device *const i2c_bus = DEVICE_DT_GET(I2C_BUS_NODE);

#define SLAVE_ADDR ((uint16_t)CONFIG_CYBERFLY_I2C_SLAVE_ADDR)

/* Register map (see cyberfly-i2c-slave-protocol.md) */
#define REG_DEVID        0x00
#define REG_FW_VER       0x01
#define REG_PROTO_VER    0x02
#define REG_STATUS       0x03
#define REG_KEY_COUNT    0x04
#define REG_FIFO_ASCII   0x05
#define REG_FIFO_EVT     0x06  /* 2 bytes: state, keycode */
#define REG_MODIFIERS    0x07
#define REG_LAYER        0x08
#define REG_SCAN         0x10  /* 12 bytes: 78-key matrix bitmap + modifiers */
#define REG_SCAN_LEN     12
#define REG_MODE         0x20
#define REG_HOLD_MS      0x21  /* 2 bytes LE */
#define REG_REPEAT_MS    0x23  /* 2 bytes LE */
#define REG_BACKLIGHT    0x30
#define REG_LED_MODE     0x31
#define REG_BATTERY      0x40
#define REG_USB_STATE    0x41
#define REG_RESET        0xF0
#define REG_FW_VER_ALT   0xF1
#define REG_HW_TYPE      0xFD
#define REG_FW_VER_M5    0xFE

#define DEVID_MAGIC      0xCF
#define FW_VER           0x10  /* v1.0 */
#define PROTO_VER        0x01
#define HW_TYPE_CYBERFLY 0xC1

/* STATUS bits */
#define STATUS_DATA_READY    BIT(0)
#define STATUS_FIFO_OVERFLOW BIT(6)
#define STATUS_ANY_PRESSED   BIT(7)

/* MODE bits */
#define MODE_FIFO_EN         BIT(0)
#define MODE_EVENT_FIFO_EN   BIT(1)
#define MODE_REPORT_HOLD     BIT(2)
#define MODE_ASCII_RELEASED  BIT(3)
#define MODE_DEFAULT         (MODE_FIFO_EN)

/* Event-FIFO state values */
#define EVT_STATE_PRESSED  1
#define EVT_STATE_HELD     2
#define EVT_STATE_RELEASED 3

#define FIFO_DEPTH CONFIG_CYBERFLY_I2C_SLAVE_FIFO_DEPTH

struct ascii_fifo {
    uint8_t buf[FIFO_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
};

struct event_fifo_entry {
    uint8_t state;
    uint8_t keycode;
};

struct event_fifo {
    struct event_fifo_entry buf[FIFO_DEPTH];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
};

struct slave_state {
    /* Register values */
    uint8_t status;
    uint8_t modifiers;
    uint8_t layer;
    uint8_t scan[REG_SCAN_LEN];   /* bytes 0..9: 78-bit bitmap, byte 10: reserved, byte 11: modifiers */
    uint8_t mode;
    uint16_t hold_ms;
    uint16_t repeat_ms;
    uint8_t backlight;
    uint8_t led_mode;
    uint8_t battery;
    uint8_t usb_state;

    /* FIFOs */
    struct ascii_fifo ascii;
    struct event_fifo events;

    /* Current read pointer */
    uint8_t read_reg;
    uint8_t read_offset;   /* offset within a multi-byte register */
    bool read_pending;     /* true if master wrote an address but hasn't read yet */

    /* Protect shared state between ZMK thread and I2C ISR */
    struct k_spinlock lock;
};

static struct slave_state g_state = {
    .hold_ms = 500,
    .repeat_ms = 300,
    .mode = MODE_DEFAULT,
    .battery = 0xFF,
    .backlight = 0,
};

/* ----- FIFO helpers ----- */

static void ascii_fifo_push(struct ascii_fifo *f, uint8_t c)
{
    if (f->count >= FIFO_DEPTH) {
        g_state.status |= STATUS_FIFO_OVERFLOW;
        /* drop newest by doing nothing; alternatively drop oldest */
        return;
    }
    f->buf[f->head] = c;
    f->head = (f->head + 1) % FIFO_DEPTH;
    f->count++;
}

static uint8_t ascii_fifo_pop(struct ascii_fifo *f)
{
    if (f->count == 0) {
        return 0;
    }
    uint8_t c = f->buf[f->tail];
    f->tail = (f->tail + 1) % FIFO_DEPTH;
    f->count--;
    return c;
}

static void event_fifo_push(struct event_fifo *f, uint8_t state, uint8_t keycode)
{
    if (f->count >= FIFO_DEPTH) {
        g_state.status |= STATUS_FIFO_OVERFLOW;
        return;
    }
    f->buf[f->head].state = state;
    f->buf[f->head].keycode = keycode;
    f->head = (f->head + 1) % FIFO_DEPTH;
    f->count++;
}

static struct event_fifo_entry event_fifo_pop(struct event_fifo *f)
{
    struct event_fifo_entry e = {0, 0};
    if (f->count == 0) {
        return e;
    }
    e = f->buf[f->tail];
    f->tail = (f->tail + 1) % FIFO_DEPTH;
    f->count--;
    return e;
}

/* ----- Public API used by listener ----- */

void cyberfly_i2c_slave_on_position(uint32_t position, bool pressed)
{
    if (position >= REG_SCAN_LEN * 8) {
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&g_state.lock);

    uint8_t byte_idx = position / 8;
    uint8_t bit_idx = position % 8;
    if (pressed) {
        g_state.scan[byte_idx] |= BIT(bit_idx);
    } else {
        g_state.scan[byte_idx] &= ~BIT(bit_idx);
    }

    bool any = false;
    for (int i = 0; i < REG_SCAN_LEN - 1; i++) {
        if (g_state.scan[i]) { any = true; break; }
    }
    if (any) {
        g_state.status |= STATUS_ANY_PRESSED;
    } else {
        g_state.status &= ~STATUS_ANY_PRESSED;
    }

    if (g_state.mode & MODE_EVENT_FIFO_EN) {
        event_fifo_push(&g_state.events,
                        pressed ? EVT_STATE_PRESSED : EVT_STATE_RELEASED,
                        (uint8_t)position);
    }

    if (g_state.ascii.count > 0 || g_state.events.count > 0) {
        g_state.status |= STATUS_DATA_READY;
    }

    k_spin_unlock(&g_state.lock, key);
}

void cyberfly_i2c_slave_on_keycode(uint16_t usage_page, uint32_t keycode,
                                    uint8_t modifiers, bool pressed)
{
    if (!(g_state.mode & MODE_FIFO_EN)) {
        return;
    }
    bool want_pressed = !(g_state.mode & MODE_ASCII_RELEASED);
    if (pressed != want_pressed) {
        return;
    }
    uint8_t ascii = cyberfly_i2c_slave_keycode_to_ascii(usage_page, keycode, modifiers);
    if (ascii == 0) {
        return;
    }
    k_spinlock_key_t key = k_spin_lock(&g_state.lock);
    ascii_fifo_push(&g_state.ascii, ascii);
    g_state.status |= STATUS_DATA_READY;
    k_spin_unlock(&g_state.lock, key);
}

void cyberfly_i2c_slave_on_modifiers(uint8_t modifiers)
{
    k_spinlock_key_t key = k_spin_lock(&g_state.lock);
    g_state.modifiers = modifiers;
    g_state.scan[REG_SCAN_LEN - 1] = modifiers;
    k_spin_unlock(&g_state.lock, key);
}

void cyberfly_i2c_slave_on_layer(uint8_t layer)
{
    g_state.layer = layer;
}

void cyberfly_i2c_slave_on_battery(uint8_t soc)
{
    g_state.battery = soc;
}

void cyberfly_i2c_slave_on_usb(uint8_t usb_state)
{
    g_state.usb_state = usb_state;
}

void cyberfly_i2c_slave_on_backlight(uint8_t brightness)
{
    g_state.backlight = brightness;
}

/* ----- Register-read snapshot builder -----
 *
 * Called at the start of each master read burst. Builds a contiguous byte
 * buffer starting at the current read_reg. The nRF TWIS buffer mode
 * requires up to CONFIG_I2C_NRFX_TWIS_BUF_SIZE bytes; we cap at 32.
 */
#define READ_SNAPSHOT_LEN 32

static uint8_t read_buf[READ_SNAPSHOT_LEN];

static uint8_t read_one(uint8_t reg)
{
    switch (reg) {
    case REG_DEVID:       return DEVID_MAGIC;
    case REG_FW_VER:      return FW_VER;
    case REG_PROTO_VER:   return PROTO_VER;
    case REG_STATUS: {
        uint8_t s = g_state.status;
        /* Clear sticky overflow on read */
        g_state.status &= ~STATUS_FIFO_OVERFLOW;
        if (g_state.ascii.count == 0 && g_state.events.count == 0) {
            g_state.status &= ~STATUS_DATA_READY;
        }
        return s;
    }
    case REG_KEY_COUNT:   return g_state.ascii.count;
    case REG_FIFO_ASCII:  return ascii_fifo_pop(&g_state.ascii);
    case REG_MODIFIERS:   return g_state.modifiers;
    case REG_LAYER:       return g_state.layer;
    case REG_MODE:        return g_state.mode;
    case REG_HOLD_MS:     return g_state.hold_ms & 0xFF;
    case REG_HOLD_MS + 1: return (g_state.hold_ms >> 8) & 0xFF;
    case REG_REPEAT_MS:   return g_state.repeat_ms & 0xFF;
    case REG_REPEAT_MS + 1: return (g_state.repeat_ms >> 8) & 0xFF;
    case REG_BACKLIGHT:   return g_state.backlight;
    case REG_LED_MODE:    return g_state.led_mode;
    case REG_BATTERY:     return g_state.battery;
    case REG_USB_STATE:   return g_state.usb_state;
    case REG_FW_VER_ALT:  return FW_VER;
    case REG_HW_TYPE:     return HW_TYPE_CYBERFLY;
    case REG_FW_VER_M5:   return FW_VER;
    default:
        /* REG_SCAN 0x10..0x1B */
        if (reg >= REG_SCAN && reg < REG_SCAN + REG_SCAN_LEN) {
            return g_state.scan[reg - REG_SCAN];
        }
        /* REG_FIFO_EVT 0x06..0x07 (2 bytes per event) */
        if (reg == REG_FIFO_EVT) {
            struct event_fifo_entry e = event_fifo_pop(&g_state.events);
            return e.state;
        }
        return 0;
    }
}

static void build_read_snapshot(uint8_t start_reg, uint32_t *len)
{
    uint32_t n = 0;
    uint8_t reg = start_reg;

    /* Two-byte event fifo: if caller reads from 0x06, emit {state, keycode}
     * as a pair per event. Simplest: produce state at 0x06, keycode at 0x07
     * by popping once for the pair and storing the keycode for the next byte. */
    static uint8_t pending_evt_keycode;
    static bool has_pending_evt;

    while (n < READ_SNAPSHOT_LEN) {
        if (reg == REG_FIFO_EVT) {
            struct event_fifo_entry e = event_fifo_pop(&g_state.events);
            read_buf[n++] = e.state;
            if (n < READ_SNAPSHOT_LEN) {
                read_buf[n++] = e.keycode;
            } else {
                pending_evt_keycode = e.keycode;
                has_pending_evt = true;
            }
            reg += 2;
            continue;
        }
        if (reg == REG_FIFO_EVT + 1 && has_pending_evt) {
            read_buf[n++] = pending_evt_keycode;
            has_pending_evt = false;
            reg += 1;
            continue;
        }
        read_buf[n++] = read_one(reg);
        reg += 1;
        if (reg == 0) break; /* wrap-around safeguard */
    }
    *len = n;
}

/* ----- Register-write handler ----- */

static void write_register(uint8_t reg, const uint8_t *data, uint32_t len)
{
    switch (reg) {
    case REG_MODE:
        if (len >= 1) g_state.mode = data[0];
        break;
    case REG_HOLD_MS:
        if (len >= 2) g_state.hold_ms = data[0] | (data[1] << 8);
        else if (len >= 1) g_state.hold_ms = (g_state.hold_ms & 0xFF00) | data[0];
        break;
    case REG_HOLD_MS + 1:
        if (len >= 1) g_state.hold_ms = (g_state.hold_ms & 0x00FF) | (data[0] << 8);
        break;
    case REG_REPEAT_MS:
        if (len >= 2) g_state.repeat_ms = data[0] | (data[1] << 8);
        else if (len >= 1) g_state.repeat_ms = (g_state.repeat_ms & 0xFF00) | data[0];
        break;
    case REG_REPEAT_MS + 1:
        if (len >= 1) g_state.repeat_ms = (g_state.repeat_ms & 0x00FF) | (data[0] << 8);
        break;
    case REG_BACKLIGHT:
        if (len >= 1) g_state.backlight = data[0];
        break;
    case REG_LED_MODE:
        if (len >= 1) g_state.led_mode = data[0];
        break;
    case REG_RESET:
        /* Clear FIFOs + sticky status */
        memset(&g_state.ascii, 0, sizeof(g_state.ascii));
        memset(&g_state.events, 0, sizeof(g_state.events));
        g_state.status = 0;
        break;
    default:
        /* unwritable / unknown -> silently ignore, like M5 */
        break;
    }
}

/* ----- I2C target callbacks (buffer mode) ----- */

static int cb_write_requested(struct i2c_target_config *cfg)
{
    /* Not used in buffer mode */
    ARG_UNUSED(cfg);
    return 0;
}

static int cb_read_requested(struct i2c_target_config *cfg, uint8_t *val)
{
    ARG_UNUSED(cfg);
    ARG_UNUSED(val);
    return 0;
}

static int cb_write_received(struct i2c_target_config *cfg, uint8_t val)
{
    ARG_UNUSED(cfg);
    ARG_UNUSED(val);
    return 0;
}

static int cb_read_processed(struct i2c_target_config *cfg, uint8_t *val)
{
    ARG_UNUSED(cfg);
    ARG_UNUSED(val);
    return 0;
}

static void cb_buf_write_received(struct i2c_target_config *cfg,
                                   uint8_t *ptr, uint32_t len)
{
    ARG_UNUSED(cfg);
    if (len == 0) {
        return;
    }
    uint8_t reg = ptr[0];

    k_spinlock_key_t key = k_spin_lock(&g_state.lock);
    g_state.read_reg = reg;
    g_state.read_offset = 0;
    g_state.read_pending = true;

    if (len > 1) {
        write_register(reg, ptr + 1, len - 1);
    }
    k_spin_unlock(&g_state.lock, key);
}

static int cb_buf_read_requested(struct i2c_target_config *cfg,
                                  uint8_t **ptr, uint32_t *len)
{
    ARG_UNUSED(cfg);

    k_spinlock_key_t key = k_spin_lock(&g_state.lock);
    uint8_t start;
    if (g_state.read_pending) {
        start = g_state.read_reg;
        g_state.read_pending = false;
    } else {
        /* Bare read with no prior write: CardKB2 1-byte ASCII pop */
        start = REG_FIFO_ASCII;
    }
    build_read_snapshot(start, len);
    k_spin_unlock(&g_state.lock, key);

    *ptr = read_buf;
    return 0;
}

static int cb_stop(struct i2c_target_config *cfg)
{
    ARG_UNUSED(cfg);
    return 0;
}

static const struct i2c_target_callbacks cyberfly_i2c_callbacks = {
    .write_requested = cb_write_requested,
    .read_requested = cb_read_requested,
    .write_received = cb_write_received,
    .read_processed = cb_read_processed,
    .buf_write_received = cb_buf_write_received,
    .buf_read_requested = cb_buf_read_requested,
    .stop = cb_stop,
};

static struct i2c_target_config cyberfly_i2c_target_cfg = {
    .flags = 0,
    .address = SLAVE_ADDR,
    .callbacks = &cyberfly_i2c_callbacks,
};

static int cyberfly_i2c_slave_init(void)
{
    if (!device_is_ready(i2c_bus)) {
        LOG_ERR("I2C bus %s not ready", i2c_bus->name);
        return -ENODEV;
    }

    int ret = i2c_target_register(i2c_bus, &cyberfly_i2c_target_cfg);
    if (ret < 0) {
        LOG_ERR("i2c_target_register failed: %d", ret);
        return ret;
    }

    LOG_INF("CyberFly I2C slave registered at 0x%02X on %s",
            SLAVE_ADDR, i2c_bus->name);
    return 0;
}

SYS_INIT(cyberfly_i2c_slave_init, APPLICATION, 90);
