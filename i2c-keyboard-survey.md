# I2C Slave Keyboard Survey

Comparison of how various vendors expose a keyboard over I2C. Verified against firmware source / upstream Linux drivers / local clones (`M5Unit-KEYBOARD/`) as of 2026-05-07.

## Summary table

| Vendor / Product | 7-bit addr | Protocol style | Read path (key event) | IRQ pin | Modifiers | FIFO depth | Authoritative source |
|---|---|---|---|---|---|---|---|
| **M5 CardKB v1 / v1.1** (SKU U035 / U035-B) | `0x5F` | Dual-mode: (a) **legacy** ASCII byte-stream, (b) **new "bitwise" fw** register map | Legacy: read 1 byte = ASCII of the just-released key, `0x00` = nothing. Bitwise fw: write reg, read: `CMD_SCAN=0x10` → 7 bytes (48 key bits LE + 1 modifier byte), `CMD_MODE=0x20` RW 1B, `CMD_HW_TYPE=0xFD` R 1B (`0x01`=CardKB, `0x11`=v1.1), `CMD_FW_VER=0xFE` R 1B (major<<4\|minor) | No | 3-level via Shift/Sym/Fn modes; Fn outputs `0x80..0xAF`, arrows `180..183`. Bitwise fw packs modifiers into high byte of scan buffer (bit0 Shift, bit1 Sym, bit2 Fn) | 1 byte (legacy) / realtime bitmap (bitwise) | local `M5Unit-KEYBOARD/examples/firmware/CardKB_Firmware/` + `src/unit/unit_CardKB.*` |
| **M5 CardKB2** (SKU U215) | `0x5F` (same as v1!) | Hybrid: **ASCII byte-stream for pressed chars**, register for firmware version only | Read 1 byte raw: `0x08` BS, `0x0A` LF, `0x0D` CR, `0x1B` ESC, `0x20-0x7E` printable, `0xB4-0xB7` arrows, `0x00` no key. `0xF1` reg = firmware version. **Cannot switch modes via I2C**: Fn+Sym+1 toggles I2C/UART at device | No | Baked into fw: modifiers already applied to emitted ASCII | 1 byte | local `src/unit/unit_CardKB2.cpp` |
| **M5 FACES QWERTY** (FACE_BOTTOM panel, 35 keys) | `0x08` | ASCII byte-stream | Read 1 byte = last key, `0x00` = no key | Board-level INT line | Baked into fw | 1 | M5Stack FACES docs + local `unit_FacesQWERTY.*` |
| **Solder Party BBQ10KBD** (PMOD, FeatherWing, BBQ10 breakout) | `0x1F` | Register map + key FIFO | Read `REG_FIF=0x09` -> 2 bytes `{state, keycode}`. Keycount in low 5 bits of `REG_KEY=0x04`. | Yes, active-low, level; config via `REG_CFG` bits; clear by resetting `REG_INT=0x03` to `0x00` | Opt-in: `CFG_REPORT_MODS` reports Shift/Alt/Sym as separate keycodes; `CFG_USE_MODS` applies them to produced codes | 31 entries (FIFO v0.3+) | https://github.com/arturo182/bbq10kbd_i2c_sw (README & protocol) |
| **Solder Party BBQ20KBD** (trackpad variant) | `0x1F` (same) | Same BBQ10 reg map + extensions | Same `REG_FIF` FIFO; adds touch/trackpad regs, GPIO expander | Yes (shared, adds `INT_TOUCH` bit6 in `REG_INT`) | Same as BBQ10, plus TP | 31 | https://github.com/solderparty/bbq20kbd_i2c_sw |
| **Adafruit NeoKey 1x4 QT** (Seesaw) | `0x30` (default; 4 addr-jumpers -> 16 addrs) | Seesaw generic register map. Keypad is NOT used; keys are read as GPIO inputs on pins 4..7 via `SEESAW_GPIO_BASE=0x01` | `digital_read_bulk(0xF0)` via Seesaw GPIO_BULK register, decode bits 4..7 | Yes, active-low, optional | None on chip — host-side | N/A (pure GPIO polling) | https://github.com/adafruit/Adafruit_CircuitPython_NeoKey |
| **Adafruit NeoTrellis 4x4** (Seesaw) | `0x2E` (default; jumpers -> many) | Seesaw keypad module: base `0x10`, regs `STATUS=0x00`, `EVENT=0x01`, `INTENSET=0x02`, `INTENCLR=0x03`, `COUNT=0x04`, `FIFO=0x10` | Read `COUNT`, then read `FIFO` N bytes. Each byte packs `{edge_type[1:0], key_num[7:2]}`, edges: HIGH/LOW/FALLING/RISING | Yes, routed via Seesaw INT | None on chip | Configurable via Seesaw | https://github.com/adafruit/Adafruit_Seesaw/blob/master/Adafruit_seesaw.h |
| **SparkFun Qwiic Keypad** (12-button) | `0x4B` (default, EEPROM-changeable); `0x4A` if jumper closed | Register map + 1-deep "current button" with host-pumped FIFO | 1) Write `0x01` to `UPDATE_FIFO=0x06`, 2) read `BUTTON=0x03` (ASCII), 3) read `TIME_MSB/LSB=0x04/0x05` (ms since press) | Yes, active-low (goes low when stack non-empty) | No concept (single layer) | 15-deep internal stack, popped via UPDATE_FIFO | https://github.com/sparkfun/Qwiic_Keypad + https://github.com/sparkfun/SparkFun_Qwiic_Keypad_Arduino_Library |
| **Pine64 PinePhone Keyboard (PPKB)** | `0x15` (+ subordinate charger at `0x75` through pass-through I2C adapter!) | Register map exposing **raw scan matrix**, not keycodes | Read `SCAN_CRC=0x07` + `SCAN_DATA=0x08..` for 1+12 bytes = CRC8(poly `0x07`) + 12 column bytes of bitmasked rows; host does matrix decode | Yes, dedicated IRQ line (edge-falling in DT), wakeup-capable | None on chip — Linux `matrix_keymap` decides modifiers | No FIFO, raw scan | drivers/input/keyboard/pinephone-keyboard.c + Documentation/devicetree/bindings/input/pine64,pinephone-keyboard.yaml |
| **Beepy / Beepberry keyboard** (RP2040 + BB Q10) | `0x1F` (same as Solder Party) | Direct fork of `bbq10kbd_i2c_sw` protocol — register-compatible | Same `REG_FIF=0x09` 2-byte event, same `REG_CFG/INT/KEY` | Yes, GPIO4 on Pi, same semantics as BBQ10 | Same as BBQ10 (plus host-side "sticky mods" & meta-mode in `beepy-kbd`) | 31 | https://github.com/ardangelo/beepberry-keyboard-driver (`src/config.h`, `src/bbqX0kbd_registers.h`) |
| **Pimoroni RGB Keypad / Keybow 2040** | N/A — **SPI keys + I2C only for LEDs (IS31FL1743/SK9822)**; not an I2C keyboard | — | — | — | — | — | Pimoroni docs |
| **ClockworkPi DevTerm / uConsole keyboards** | N/A — **USB HID** (internal ATmega32U4 / STM32 speaks USB, not I2C) | — | — | — | — | — | ClockworkPi firmware repos |
## Register map detail — M5 CardKB bitwise firmware (0x5F)

Write = `{cmd_byte}`, read (after write) or raw read = bytes. `CMD_MODE` write takes a 2-byte message `{0x20, scan_mode}` where `scan_mode=1` switches from legacy ASCII-release to bitwise-pressed mode.

| Reg | ID | Size | Purpose |
|---|---|---|---|
| `CMD_SCAN` | `0x10` | 7B R | 48-bit key-press bitmap LE (`bit i` = key index `i` pressed) + byte 6 = modifier bitmap (bit0 Shift, bit1 Sym, bit2 Fn) |
| `CMD_MODE` | `0x20` | 1B RW | `0` legacy, `1` bitwise/scan mode |
| `CMD_HW_TYPE` | `0xFD` | 1B R | `0x01` CardKB (ATmega328), `0x11` CardKB v1.1 (ATmega8A) |
| `CMD_FW_VER` | `0xFE` | 1B R | High nibble major, low nibble minor (`0x01` = v0.1) |

Key layout (48 keys, index 0..47): Esc, 1..0, BS, Tab, QWERTYUIOP, (no-key@23), Left, Up, ASDFGHJKL, Enter, Down, Right, ZXCVBNM, `,`, `.`, Space.

Legacy fallback behavior (used by older firmware and as the "Conventional" mode): master just reads 1 byte at any time; the keyboard returns the ASCII of the *released* key since the last read, applying whatever modifier mode was active at release time. Non-ASCII function keys use `0x80 + key_index`, arrows use `180..183`.

## Register map detail — M5 CardKB2 (0x5F)

There is no register map for key events. Master does `Wire.requestFrom(0x5F, 1)` and gets one byte:

- `0x00` → no key
- `0x08, 0x0A, 0x0D, 0x1B, 0x20..0x7E` → valid key (BS, LF, CR, ESC, printable)
- anything else → bogus (filter it; the official driver literally does this because `0x01` is observed spuriously on AtomS3)
- `0xB4..0xB7` → arrow keys (Left/Up/Down/Right)

Only `0xF1` exists as a "register" for firmware version. Unlike CardKB v1, there is **no way to put CardKB2 into a matrix-bits mode over I2C**; the user must physically press Fn+Sym+1 on the keyboard.

## Register map detail — Solder Party BBQ10KBD (the other "standard")

Write access = OR the register ID with **`0x80`** (e.g. write backlight = `0x85`). Read access = send register ID byte, then I2C read N bytes.

| Reg | ID | Size | Purpose |
|---|---|---|---|
| REG_VER | `0x01` | 1B | Firmware version: high nibble major, low nibble minor |
| REG_CFG | `0x02` | 1B RW | Bit7 `USE_MODS`, Bit6 `REPORT_MODS`, Bit5 `PANIC_INT`, Bit4 `KEY_INT`, Bit3 `NUMLOCK_INT`, Bit2 `CAPSLOCK_INT`, Bit1 `OVERFLOW_INT`, Bit0 `OVERFLOW_ON`. Default: `KEY_INT|OVERFLOW_INT|USE_MODS` |
| REG_INT | `0x03` | 1B RW | IRQ cause bitmap. `INT_KEY=b3`, `INT_NUMLOCK=b2`, `INT_CAPSLOCK=b1`, `INT_OVERFLOW=b0`. Must be manually cleared to `0x00` after read |
| REG_KEY | `0x04` | 1B R | Bit6 NumLock state, Bit5 CapsLock state, Bits0-4 `KEY_COUNT` (items in FIFO, max 31) |
| REG_BKL | `0x05` | 1B RW | Backlight PWM 0x00..0xFF, default `0xFF` |
| REG_DEB | `0x06` | 1B RW | Debounce (not implemented, default 10) |
| REG_FRQ | `0x07` | 1B RW | Poll frequency (not implemented, default 5) |
| REG_RST | `0x08` | — | Any access triggers software reset |
| REG_FIF | `0x09` | 2B R | `{key_state, key_code}`. State: `1=Pressed`, `2=Held`, `3=Released` |

BBQ20 extends this (touchpad regs, GPIO expander, version reg `0x10`) but the event-read API is unchanged — this is why Beepy can reuse the exact same driver.

## What the community standardized on vs what is idiosyncratic

**Standardized (3 of the 4 "serious" keyboards use this shape):**
- **Read-latest-event FIFO at a fixed register** (`REG_FIF=0x09` on BBQ10/Beepy, `SEESAW_KEYPAD_FIFO=0x10` on NeoTrellis, `UPDATE_FIFO + BUTTON` on SparkFun). The repeating pattern is: a *count* register so host knows how many to pop, then a FIFO register that returns `{event_type, key_code}` tuples.
- **Active-low, open-drain IRQ pin** with "line is low while data is waiting". All of BBQ10, NeoTrellis, SparkFun Qwiic Keypad, and PPKB behave this way.
- **Write-bit = OR `0x80`** on the 7-bit register id (BBQ10, Beepy). This is a BlackBerry-style convention, not an I2C standard — Seesaw and SparkFun don't do this; they just have separate writable register indices.
- **2-byte event format `{state/edge, keycode}`** on both BBQ10 (`state ∈ {1,2,3}`) and Seesaw NeoTrellis (`edge ∈ {HIGH,LOW,FALLING,RISING}`, packed in bits 1:0). SparkFun packs differently: `{ASCII, time_MSB, time_LSB}` at separate regs.

**Idiosyncratic per vendor:**
- **PinePhone** ships *raw scan matrix* over I2C, not keycodes — the host builds keycodes. Also uniquely carries a subordinate I2C bus to the charger (`0x75`) tunnelled over SMBus passthrough commands (`SYS_COMMAND 0x23`, `0x91`=read/`0xa1`=write). No one else does that.
- **Seesaw** is a full-blown general-purpose MCU peripheral bus — keypad is just module `0x10` alongside GPIO, NeoPixel, ADC, Timer, Encoder, etc. So NeoKey 1x4 doesn't even use the keypad module; it polls GPIO pins 4..7 as "buttons".
- **M5 CardKB2 / FACES QWERTY** return ASCII bytes with no register map at all — an extreme minimalist protocol. Opposite end of the spectrum from PPKB. M5 CardKB v1 straddles both: legacy ASCII **or** a 7-byte register-based scan bitmap if you flash the "M5UnitUnified" firmware.
- **SparkFun** is the only one requiring an explicit *pump command* (`UPDATE_FIFO=0x06` bit0) before reading the next event — everyone else auto-advances on read.
- **Address placement is all over the place**: `0x08` (FACES), `0x15` (PPKB), `0x1F` (BBQ10/Beepy), `0x2E` (NeoTrellis), `0x30` (NeoKey), `0x4B` (SparkFun), `0x49` (generic Seesaw), `0x5F` (M5 CardKB/CardKB2). No convergence.
- **Modifier handling**: only BBQ10/Beepy has a real modifier concept on the slave (`CFG_USE_MODS` vs `CFG_REPORT_MODS`). M5 CardKB v1 bitwise fw also exposes a modifier byte. PPKB punts to host. Seesaw/SparkFun/CardKB2 don't model modifiers at all.

## Head-to-head: M5 CardKB vs BBQ10

| Axis | M5 CardKB v1 (bitwise) | M5 CardKB2 | BBQ10KBD |
|---|---|---|---|
| Addr | 0x5F | 0x5F | 0x1F |
| Default read | 1B ASCII (released key) | 1B ASCII (pressed key) | `{state,key}` 2B from `REG_FIF` |
| Full matrix state | 7B @ `0x10` | N/A | N/A (events only) |
| Modifiers | 3 (Shift/Sym/Fn) in high byte of scan | none | 5 (Shift L/R, Alt, Ctrl, Sym) as keycodes |
| IRQ | no | no | yes, active-low |
| FW version reg | `0xFE` | `0xF1` | `0x01` (`REG_VER`) |
| Write cmd bit | plain reg id | plain reg id | OR `0x80` |
| Key FIFO | no | no | 31-deep |
| Good for | "which keys are down right now" polling | dead-simple ASCII input | async event streaming |

## Takeaway for keebdeck / CyberFly

Two equally reasonable standards to mimic:
1. **BBQ10 at 0x1F** — event-FIFO + IRQ line, richest, most cyberdeck-oriented; but requires a spare GPIO for IRQ and is write-bit-quirky.
2. **M5 CardKB v1 bitwise fw at 0x5F** — stateless 7-byte scan bitmap poll, no IRQ, no write-bit hackery, already supported by the M5UnitUnified library. Dumbest code on the slave side.

A third option is to **support both** by responding to whichever register the master addresses first — the register IDs (`0x04/0x09` vs `0x10/0x20/0xFE`) don't collide and the address can be chosen by a config jumper or at build time. That is what I recommend below for CyberFly: run at `0x5F` (M5-compatible) by default, optionally `0x1F` (BBQ10-compatible), and expose a superset register map.
