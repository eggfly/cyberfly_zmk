/*
 * HID keycode -> ASCII lookup for the CyberFly I2C slave.
 *
 * Table layout is keyed by HID usage ID on page 0x07 (Keyboard/Keypad).
 * Column 0 = normal, column 1 = shift (US QWERTY).
 * Unmapped keys return 0.
 *
 * Special-key values use M5 CardKB conventions:
 *   0x08 BS, 0x09 TAB, 0x0A LF, 0x0D CR, 0x1B ESC
 *   180/181/182/183 = Left/Up/Down/Right arrows
 */

#include <stdint.h>

#define HID_KC_MAX 0x70

static const uint8_t ascii_map[HID_KC_MAX][2] = {
    [0x00] = {0,    0},
    [0x01] = {0,    0},    /* ErrorRollOver */
    [0x02] = {0,    0},
    [0x03] = {0,    0},
    [0x04] = {'a',  'A'},
    [0x05] = {'b',  'B'},
    [0x06] = {'c',  'C'},
    [0x07] = {'d',  'D'},
    [0x08] = {'e',  'E'},
    [0x09] = {'f',  'F'},
    [0x0A] = {'g',  'G'},
    [0x0B] = {'h',  'H'},
    [0x0C] = {'i',  'I'},
    [0x0D] = {'j',  'J'},
    [0x0E] = {'k',  'K'},
    [0x0F] = {'l',  'L'},
    [0x10] = {'m',  'M'},
    [0x11] = {'n',  'N'},
    [0x12] = {'o',  'O'},
    [0x13] = {'p',  'P'},
    [0x14] = {'q',  'Q'},
    [0x15] = {'r',  'R'},
    [0x16] = {'s',  'S'},
    [0x17] = {'t',  'T'},
    [0x18] = {'u',  'U'},
    [0x19] = {'v',  'V'},
    [0x1A] = {'w',  'W'},
    [0x1B] = {'x',  'X'},
    [0x1C] = {'y',  'Y'},
    [0x1D] = {'z',  'Z'},
    [0x1E] = {'1',  '!'},
    [0x1F] = {'2',  '@'},
    [0x20] = {'3',  '#'},
    [0x21] = {'4',  '$'},
    [0x22] = {'5',  '%'},
    [0x23] = {'6',  '^'},
    [0x24] = {'7',  '&'},
    [0x25] = {'8',  '*'},
    [0x26] = {'9',  '('},
    [0x27] = {'0',  ')'},
    [0x28] = {0x0D, 0x0D}, /* Enter */
    [0x29] = {0x1B, 0x1B}, /* Esc */
    [0x2A] = {0x08, 0x08}, /* Backspace */
    [0x2B] = {0x09, 0x09}, /* Tab */
    [0x2C] = {' ',  ' '},
    [0x2D] = {'-',  '_'},
    [0x2E] = {'=',  '+'},
    [0x2F] = {'[',  '{'},
    [0x30] = {']',  '}'},
    [0x31] = {'\\', '|'},
    [0x32] = {'#',  '~'}, /* Non-US # and ~ */
    [0x33] = {';',  ':'},
    [0x34] = {'\'', '"'},
    [0x35] = {'`',  '~'},
    [0x36] = {',',  '<'},
    [0x37] = {'.',  '>'},
    [0x38] = {'/',  '?'},
    [0x39] = {0,    0},   /* CapsLock */
    /* F1..F12 = 0x3A..0x45, return 0 (no ASCII) */
    [0x4C] = {0x7F, 0x7F}, /* Delete */
    [0x4F] = {183,  183},  /* Right arrow */
    [0x50] = {180,  180},  /* Left arrow */
    [0x51] = {182,  182},  /* Down arrow */
    [0x52] = {181,  181},  /* Up arrow */
    [0x53] = {0,    0},    /* NumLock */
    [0x54] = {'/',  '/'},  /* Keypad / */
    [0x55] = {'*',  '*'},
    [0x56] = {'-',  '-'},
    [0x57] = {'+',  '+'},
    [0x58] = {0x0D, 0x0D}, /* Keypad Enter */
    [0x59] = {'1',  '1'},
    [0x5A] = {'2',  '2'},
    [0x5B] = {'3',  '3'},
    [0x5C] = {'4',  '4'},
    [0x5D] = {'5',  '5'},
    [0x5E] = {'6',  '6'},
    [0x5F] = {'7',  '7'},
    [0x60] = {'8',  '8'},
    [0x61] = {'9',  '9'},
    [0x62] = {'0',  '0'},
    [0x63] = {'.',  '.'},
};

uint8_t cyberfly_i2c_slave_keycode_to_ascii(uint16_t usage_page, uint32_t keycode,
                                             uint8_t modifiers)
{
    if (usage_page != 0x07) {
        return 0;
    }
    if (keycode >= HID_KC_MAX) {
        return 0;
    }
    /* MOD_LSFT = 0x02, MOD_RSFT = 0x20 */
    uint8_t shift = (modifiers & (0x02 | 0x20)) ? 1 : 0;
    return ascii_map[keycode][shift];
}
