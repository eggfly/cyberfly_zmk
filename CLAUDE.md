# CyberFly ZMK Firmware

## Build Instructions

Board name: `nice_nano/nrf52840/zmk` (three-part name with ZMK variant suffix)

### Production build (CyberFly shield)

```bash
cd /Users/eggfly/github/keebdeck/cyberfly_zmk
rm -rf build && .venv/bin/west build -s app -p -b nice_nano/nrf52840/zmk -- -DSHIELD=cyberfly
```

### Debug build (with USB logging)

```bash
rm -rf build && .venv/bin/west build -s app -p -b nice_nano/nrf52840/zmk -S zmk-usb-logging -- -DSHIELD=cyberfly_keyboard
```

### Incremental build

```bash
.venv/bin/west build -s app
```

### Output

UF2 file: `build/zephyr/zmk.uf2`

## Notes

- Use `.venv/bin/west` directly, no need to `source .venv/bin/activate`
- Do NOT use `nice_nano_v2` or `nice_nano/nrf52840` as board name (will fail with devicetree errors)
- USB logging snippet: `-S zmk-usb-logging` (not `-DSNIPPET=`)
- Incremental builds reuse the cached shield config

## Flash via JLink (bypass USB)

```bash
cd /Users/eggfly/github/keebdeck/keebdeck_ble/jlink-scripts
echo "y" | ./05-flash-hex.sh ../firmware/cyberfly_zmk_full_3v3.hex
```

## Flash via UF2

Drag `build/zephyr/zmk.uf2` to the NICENANO USB drive.
