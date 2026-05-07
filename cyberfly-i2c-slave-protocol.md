# CyberFly I2C Slave Protocol

Design note for exposing the CyberFly (keebdeck nRF52840) keyboard as an I2C slave so an external host MCU can poll/listen for key events. Companion doc: `i2c-keyboard-survey.md`.

## Scenario

CyberFly board is already a USB + BLE HID keyboard on its own. We want an extra "wearable" mode: power the keyboard from 3.3 V, tie its I2C pins to a host MCU (ESP32, RP2040, whatever), and let the host pull keypresses for its own UI — without touching USB/BLE.

Target: the nRF52840 acts as an **I2C target device** (`nordic,nrf-twis`). CyberFly's existing ZMK `kscan` layer already produces `zmk_position_state_changed` and `zmk_keycode_state_changed` events; we add a listener that snapshots state into an I2C register window.

## Design choices

### Addressing

Default **`0x5F`** (M5 CardKB-compatible). Selectable at build time via Kconfig (`CYBERFLY_I2C_SLAVE_ADDR`). Optional fallback `0x1F` (BBQ10-compatible) for cyberdeck-style hosts.

Rationale: `0x5F` lets existing M5 CardKB drivers (M5Unit-KEYBOARD, UIFlow, CircuitPython `m5-cardkb`) talk to CyberFly with zero host-side code changes, which is the most valuable interop target given the user's M5 ecosystem. BBQ10 is wire-level mutually exclusive (write-bit `0x80`, reg `0x09`), so it's an either/or build flag rather than a runtime overlay.

### Protocol shape

**Superset of M5 CardKB bitwise firmware**, because (a) CyberFly's 6×13 matrix is bigger than CardKB's 48 keys, (b) we want to expose more than ASCII, and (c) we don't have an IRQ line wired by default.

Core idea: a **register window** that the master reads after writing the register index. Key events are exposed two ways simultaneously:
- **Bitmap view** (M5-style, stateless): master reads current "which keys are down" matrix. Good for polling.
- **ASCII FIFO view** (CardKB2-style, simple): master reads 1 byte → oldest un-read typed character. `0x00` = nothing.

### Register map

All multi-byte values little-endian. Writes with no data act as "set read pointer". Writes with data = configuration.

| Reg | Name | Size | R/W | Description |
|---|---|---|---|---|
| `0x00` | `REG_DEVID` | 1B | R | Magic `0xCF` (CyberFly) |
| `0x01` | `REG_FW_VER` | 1B | R | High nibble major, low nibble minor (start at `0x10` = v1.0) |
| `0x02` | `REG_PROTO_VER` | 1B | R | Protocol revision. `0x01` for this doc |
| `0x03` | `REG_STATUS` | 1B | R | Bits: `b7`=any_pressed, `b6`=fifo_overflow (sticky, clear on read), `b5..b1`=reserved, `b0`=data_ready (fifo non-empty) |
| `0x04` | `REG_KEY_COUNT` | 1B | R | Number of ASCII chars waiting in FIFO (0..31), BBQ10-compatible nibble layout optional |
| `0x05` | `REG_FIFO_ASCII` | 1B | R | Pop one ASCII/extended byte from FIFO (same value space as M5 CardKB2: `0x08`/`0x0A`/`0x0D`/`0x1B`/`0x20..0x7E`/`0xB4..0xB7` arrows). `0x00` = empty |
| `0x06` | `REG_FIFO_EVT` | 2B | R | Pop one `{state, keycode}` event, BBQ10-style. `state`: `1`=pressed, `2`=held, `3`=released. `keycode` = CyberFly key index (0..77) |
| `0x07` | `REG_MODIFIERS` | 1B | R | Current modifier bitmap: `b0` Shift, `b1` Sym/AltGr, `b2` Fn, `b3` Alt, `b4` Ctrl, `b5` GUI/Meta, `b6..7` reserved |
| `0x08` | `REG_LAYER` | 1B | R | Active ZMK layer index (0..7) |
| `0x10` | `REG_SCAN` | 12B | R | Key-press bitmap. `6 rows × 13 cols = 78 keys`. Byte `i` contains bits `[8i..8i+7]`. Byte 10..11 carry overflow bits + modifier byte at byte 11 (Shift `b0`, Sym `b1`, Fn `b2`) for CardKB-compat readers that grab 7 bytes starting at `0x10` — those get the first 48 bits and a modifier byte, matching CardKB's wire format |
| `0x20` | `REG_MODE` | 1B | RW | `b0` fifo_enable (default 1), `b1` event_fifo_enable (default 0, uses reg 0x06), `b2` report_hold (emit `state=2` after hold threshold), `b3` ascii_released (CardKB legacy: FIFO gets released-key ASCII instead of pressed-key ASCII) |
| `0x21` | `REG_HOLD_MS` | 2B | RW | Hold threshold (ms, default 500) |
| `0x23` | `REG_REPEAT_MS` | 2B | RW | Repeat threshold (ms, default 300); `0` disables |
| `0x30` | `REG_BACKLIGHT` | 1B | RW | PWM 0..255, mirrors ZMK backlight |
| `0x31` | `REG_LED_MODE` | 1B | RW | Reserved for RGB status LED control |
| `0x40` | `REG_BATTERY` | 1B | R | Battery percent 0..100, `0xFF` if unavailable |
| `0x41` | `REG_USB_STATE` | 1B | R | `b0` USB connected, `b1` BLE connected |
| `0xF0` | `REG_RESET` | 1B | W | Any value → software reset of the I2C state machine (clears FIFO, not the whole chip) |
| `0xF1` | `REG_FW_VER_ALT` | 1B | R | Alias of `REG_FW_VER`, for CardKB2 host-library compatibility |
| `0xFD` | `REG_HW_TYPE` | 1B | R | `0xC1` — NOT `0x01`/`0x11` so M5 drivers that strictly check hardware type will reject us and fall back to legacy ASCII mode. This is intentional — it prevents an M5UnitUnified host from reading 7-byte scans off of our 12-byte matrix and misinterpreting them |
| `0xFE` | `REG_FW_VER_M5` | 1B | R | Alias of `REG_FW_VER`, for M5 CardKB bitwise-fw host-library compatibility |

**Backwards-compat escape hatch for "just read 1 byte"**: if the master does a raw read without first writing a register address, CyberFly returns the next byte from `REG_FIFO_ASCII` (same as CardKB2 behavior). This is the "dumbest" path and what will work with a one-line `Wire.requestFrom(0x5F, 1)` call.

### Optional DATA_READY pin

Add a single GPIO (active-low, open-drain with external pull-up) asserted while FIFO non-empty. Board-dependent. Named `cyberfly,i2c-slave-int-gpios` in devicetree, optional. If unwired, host must poll. BBQ10-style clients expect this behavior; M5 clients don't use it.

## Mapping from ZMK events to register state

- `zmk_position_state_changed` (`position` = 0..77, `state`=pressed/released):
  - update the 12-byte scan bitmap at reg `0x10`
  - push a `{state, keycode=position}` tuple into the 31-deep event FIFO (reg `0x06`)
- `zmk_keycode_state_changed` (HID usage page + keycode + state):
  - derive ASCII from HID keycode + current modifiers via a small lookup table (same shape as M5 CardKB's `key_map[][4]` — normal/shift/sym/fn)
  - push ASCII into the ASCII FIFO (reg `0x05`) on **press** by default; on **release** if `ascii_released` bit set
- `zmk_modifiers_state_changed`: update `REG_MODIFIERS` (`0x07`)
- `zmk_layer_state_changed`: update `REG_LAYER` (`0x08`)
- backlight / battery: just mirror into regs on change

FIFO overflow: if the FIFO is full, newest event is dropped and `STATUS.b6` goes sticky until read.

## CyberFly matrix → key index

Position = `row * 13 + col`, range 0..77. This is the raw ZMK position, so index meaning is determined by the user's keymap — which is fine because the host reads a keymap-agnostic position + the resolved ASCII FIFO separately.

## ZMK implementation plan

### New files (under `cyberfly_zmk/app/src/`)

1. **`i2c_slave.c`** — Zephyr I2C target driver glue (`i2c_target_register`, callbacks `write_requested`, `write_received`, `read_requested`, `read_processed`, `stop`). Holds the register file, FIFOs, and a k_work for ZMK event → register state updates.
2. **`i2c_slave_keymap.c`** — HID-keycode-to-ASCII lookup (Shift/Sym/Fn columns, like `key_map[][4]` in CardKB firmware). Kept minimal; covers US layout only in v1.
3. **`i2c_slave_listener.c`** — `ZMK_SUBSCRIPTION` to `zmk_position_state_changed`, `zmk_keycode_state_changed`, `zmk_modifiers_state_changed`, `zmk_layer_state_changed`, `zmk_battery_state_changed`, `zmk_usb_conn_state_changed`, `zmk_ble_active_profile_changed`.

### New config files

4. **`app/boards/shields/cyberfly_keyboard/Kconfig.defconfig`** additions:
```
config CYBERFLY_I2C_SLAVE
    bool "Expose keys as I2C slave (CardKB/BBQ10-compatible)"
    default n
    depends on I2C
    select I2C_TARGET

config CYBERFLY_I2C_SLAVE_ADDR
    hex "I2C slave address (7-bit)"
    default 0x5F
    depends on CYBERFLY_I2C_SLAVE
    range 0x08 0x77

config CYBERFLY_I2C_SLAVE_FIFO_DEPTH
    int "FIFO depth for ASCII + event queue"
    default 31
    depends on CYBERFLY_I2C_SLAVE
    range 4 127
```

5. **`app/boards/shields/cyberfly_keyboard/boards/nice_nano_nrf52840_zmk.overlay`** — add:
```dts
/* I2C1 as slave: SDA=P0.04, SCL=P0.05 (per debug-io.md) */
&pinctrl {
    i2c1_target_default: i2c1_target_default {
        group1 {
            psels = <NRF_PSEL(TWIS_SDA, 0, 4)>,
                    <NRF_PSEL(TWIS_SCL, 0, 5)>;
        };
    };
};

&i2c1 {
    compatible = "nordic,nrf-twis";
    status = "okay";
    pinctrl-0 = <&i2c1_target_default>;
    pinctrl-names = "default";
    address-0 = <0x5F>;  /* overridden by Kconfig at build */
};
```

Notes: I2C0 is already in use for the QMI8658A (SDA=P0.26/SCL=P1.09), so slave lives on I2C1 using the spare `P0.04/P0.05` pair already identified in `debug-io.md`. TWIM and TWIS are different peripherals on nRF52840 — no conflict.

6. **`app/src/CMakeLists.txt`** — conditionally add `i2c_slave*.c` under `if(CONFIG_CYBERFLY_I2C_SLAVE)`.

### nRF52840 I2C target caveats

- Use `nordic,nrf-twis` (TWIS, hardware target). Supports **one** 7-bit address per peripheral; the two-address support is marketing for I2C1 only — fine.
- TWIS is EasyDMA-driven. Zephyr's `i2c_target` callbacks mask that well — in `read_requested` / `read_processed`, return a single byte per call; Zephyr handles the DMA setup.
- Clock-stretching: nRF52 TWIS stretches SCL between the register-addr write and the read, so a multi-byte read like "write `0x10`, read 12 bytes" works even if our ISR is slow. Host must tolerate ≤200 µs stretch.
- Power: TWIS wakes on START, so deep-sleep works. `CONFIG_PM_DEVICE=y` should just work; suspend/resume hooks handled by the Zephyr driver.

### Open questions / followups

- **Pull-ups**: per existing `debug-io.md`, P0.04/P0.05 have 10K pull-ups DNP by default. If used as slave, the host side must provide them, or we flip them populated. Document this in the shield README.
- **Matrix size quirk**: CyberFly's 78 keys don't fit in the 48-bit CardKB format. The 12-byte `REG_SCAN` is a superset; an M5 host reading 7 bytes gets keys 0..47 only (rows 0..3 + most of row 4), which is a clean degradation.
- **Key repeat / hold detection**: reuse the M5UnitUnified approach (threshold-based, done in our I2C layer, not depending on ZMK's own repeat behavior).
- **I2C address conflicts**: `0x5F` is the same as MPR121 capsense and a few humidity sensors. If the host bus has one, override via Kconfig.
- **Follow-up doc**: once implemented, write a `host-examples/` folder with Arduino/MicroPython snippets demonstrating both the 1-byte-ASCII and 12-byte-matrix read paths.

## Implementation status (2026-05-07)

Implemented and shipping. Full register map live. Files:
- `app/src/i2c_slave.c` — Zephyr I2C target driver, register file, ASCII + event FIFOs
- `app/src/i2c_slave_keymap.c` — HID keycode → ASCII (US QWERTY, shift column)
- `app/src/i2c_slave_listener.c` — ZMK event subscriptions
- `app/boards/shields/cyberfly_keyboard/boards/nice_nano_nrf52840_zmk.overlay` — rebinds `i2c1` from `nordic,nrf-twi` to `nordic,nrf-twis`, pins SDA=P0.04 / SCL=P0.05, adds `cyberfly-i2c-slave-bus` alias
- `app/boards/shields/cyberfly_keyboard/Kconfig.defconfig` — `CYBERFLY_I2C_SLAVE`, `CYBERFLY_I2C_SLAVE_ADDR` (default `0x5F`), `CYBERFLY_I2C_SLAVE_FIFO_DEPTH` (default 31)

`cyberfly_keyboard.conf` has `CONFIG_CYBERFLY_I2C_SLAVE=y` so every CyberFly firmware from this point on ships with the slave interface enabled. Flash footprint: **34.20 %** (277 KB / 792 KB), RAM **25.50 %** (67 KB / 256 KB).

Build command (used for the shipped uf2):
```
cd cyberfly_zmk
rm -rf build
ZEPHYR_SDK_INSTALL_DIR=~/zephyr-sdk \
  .venv/bin/west build -s app -b nice_nano/nrf52840/zmk -S zmk-usb-logging \
  -- -DSHIELD=cyberfly_keyboard
```
Output: `build/zephyr/zmk.uf2` (≈ 542 KB). Packaged as `cyberfly_zmk/cyberfly_i2c_slave_<timestamp>.uf2`.

## Host-side example — dumbest path (M5 CardKB compatible)

```c
#include <Wire.h>
#define CYBERFLY_ADDR 0x5F

void setup() {
    Wire.begin();
    Serial.begin(115200);
}

void loop() {
    Wire.requestFrom(CYBERFLY_ADDR, 1);
    if (Wire.available()) {
        uint8_t c = Wire.read();
        if (c) Serial.write(c);   // ASCII-like byte, or 180..183 for arrows
    }
    delay(10);
}
```

## Host-side example — full matrix poll

```c
uint8_t scan[12];
Wire.beginTransmission(CYBERFLY_ADDR);
Wire.write(0x10);                 // REG_SCAN
Wire.endTransmission(false);
Wire.requestFrom(CYBERFLY_ADDR, 12);
for (int i = 0; i < 12 && Wire.available(); i++) scan[i] = Wire.read();
uint8_t modifiers = scan[11];     // b0 Shift, b1 Sym, b2 Fn
// scan[0..9] is a 78-key bitmap, bit N of byte floor(N/8) = key index N
```

## Host-side example — BBQ10-style event FIFO

```c
// Poll STATUS (0x03) for data_ready, then drain events
uint8_t status;
Wire.beginTransmission(CYBERFLY_ADDR);
Wire.write(0x03);
Wire.endTransmission(false);
Wire.requestFrom(CYBERFLY_ADDR, 1);
status = Wire.read();

if (status & 0x01) {              // STATUS_DATA_READY
    // enable event FIFO once: write 0x20 with value (MODE_FIFO_EN|MODE_EVENT_FIFO_EN) = 0x03
    Wire.beginTransmission(CYBERFLY_ADDR);
    Wire.write(0x06);             // REG_FIFO_EVT
    Wire.endTransmission(false);
    Wire.requestFrom(CYBERFLY_ADDR, 2);
    uint8_t state = Wire.read();  // 1=pressed, 2=held, 3=released
    uint8_t keycode = Wire.read();// position index 0..77
}
```
