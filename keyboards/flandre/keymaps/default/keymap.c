/* Copyright 2020 Ruby Lazuli
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "config.h"

// Modes:
// * Steno - For stenographic input. Type by phonetics.
// * Alpha - For alphabetic input. Type by spelling. Has a number layer.
typedef enum flandre_mode {
    FS_STENO_MODE,
    FS_ALPHA_MODE
} flandre_mode;

// Bitmask for determining if the mode switch combo was pressed
typedef enum flandre_mode_switch_key {
    FS_DQ_PRESS = 1,
    FS_OP_PRESS = 2,
    FS_EM_PRESS = 4,
    FS_QM_PRESS = 8,
    FS_MODE_SWITCH_COMBO = FS_DQ_PRESS | FS_OP_PRESS | FS_EM_PRESS | FS_QM_PRESS
} flandre_mode_switch_key;

static flandre_mode_switch_key fs_switch_mask = 0;
static flandre_mode fs_mode = FS_STENO_MODE;
static uint8_t fs_th_last = 0;
static uint16_t fs_th_timer = 0;

// Layers:
// * Steno - For steno mode.
// * Alpha - For alpha mode.
// * Number - For numeric input in alpha mode.
enum flandre_layer {
    FS_STENO_LAYER,
    FS_ALPHA_LAYER,
    FS_NUMBR_LAYER
};

enum flandre_keycode {
    FS_LNGA = KC_F13, // long a
    FS_LNGO = KC_F14, // long o
    FS_OO = KC_F15, // う
    FS_LNGE = KC_F16, // long e
    FS_DQT_SQH = SAFE_RANGE, // double quote tap, single quote hold
    FS_OPT_CPH, // open paren tap, close paren hold
    FS_EMT_SMH, // exclamation mark tap, semicolon hold
    FS_QMT_CLH, // question mark tap, colon hold
    FS_PDT_CMH, // period tap, comma hold
};

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [FS_STENO_LAYER] = LAYOUT(
        KC_D,    KC_S,    KC_T, KC_P, KC_H,                         KC_F, KC_M, KC_L,  KC_J,    KC_BSPC, 
        KC_Z,    KC_X,    KC_K, KC_W, KC_R,                         KC_N, KC_B, KC_G,  KC_V,    KC_ENT, 
        FS_LNGA, FS_LNGO, KC_I, KC_A, KC_O, FS_DQT_SQH, FS_EMT_SMH, KC_E, KC_U, FS_OO, FS_LNGE, FS_PDT_CMH, 
                                            FS_OPT_CPH, FS_QMT_CLH
    ),

    [FS_ALPHA_LAYER] = LAYOUT(
        KC_Q, KC_W, KC_E, KC_R, KC_T,                         KC_Y, KC_U, KC_I, KC_O,               KC_BSPC, 
        KC_A, KC_S, KC_D, KC_F, KC_G,                         KC_H, KC_J, KC_K, KC_L,               KC_ENT, 
        KC_Z, KC_X, KC_C, KC_V, KC_B, FS_DQT_SQH, FS_EMT_SMH, KC_N, KC_M, KC_P, MO(FS_NUMBR_LAYER), FS_PDT_CMH, 
                                      FS_OPT_CPH, FS_QMT_CLH
    ),

    [FS_NUMBR_LAYER] = LAYOUT(
        KC_1,    KC_2,    KC_3,    KC_4,    KC_5,                      KC_6,    KC_7,    KC_8,    KC_9,    KC_BSPC, 
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS,                   KC_TRNS, KC_TRNS, KC_TRNS, KC_0,    KC_TRNS, 
        KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_TRNS, KC_0,    KC_TRNS, KC_TRNS, 
                                                     KC_TRNS, KC_TRNS
    ),
};

/* Handle a tap-hold key
 * Like mod tap or space cadet shift, but generalized
 * Note that tap keycode should not be [just] a modifier since that doesn't make much sense
 * Also note that the original keycode is an 8-bit simple keycode
 */
void perform_tap_hold(bool pressed, uint16_t tapKeycode, uint16_t holdKeycode, uint8_t origKeycode) {
    if (pressed) {
        // Key pressed, record this and activate hold key if it's a modifier
        fs_th_last = origKeycode;
        fs_th_timer = timer_read();
        if (IS_MOD(holdKeycode)) {
            register_mods(MOD_BIT(holdKeycode));
        }
    } else {
        if (fs_th_last == origKeycode && timer_elapsed(fs_th_timer) < TAPPING_TERM) {
            // Key tapped, tap the tap keycode
            if (IS_MOD(holdKeycode)) {
                unregister_mods(MOD_BIT(holdKeycode));
            }
            tap_code16(tapKeycode);
        } else {
            // Key held, tap the hold keycode
            if (IS_MOD(holdKeycode)) {
                // The hold mod is already held; release it
                unregister_mods(MOD_BIT(holdKeycode));
            } else {
                tap_code16(holdKeycode);
            }
        }
    }
}

static inline void toggle_mode(void) {
    // layer_move clears all layers before applying the given layer,
    // which should turn off the number layer if it's on
    if (fs_mode == FS_STENO_MODE) {
        layer_move(FS_ALPHA_LAYER);
        fs_mode = FS_ALPHA_MODE;
    } else {
        layer_move(FS_STENO_LAYER);
        fs_mode = FS_STENO_MODE;
    }
}

bool handle_layer_switch_combo(flandre_mode_switch_key key, bool pressed) {
    // Check to see if the layer switch combo is pressed ("(!?)
    if (pressed) {
        // This key is pressed, record that
        fs_switch_mask |= key;
        if (fs_switch_mask == FS_MODE_SWITCH_COMBO) {
            // We've pressed the layer switch combo, switch layers
            fs_switch_mask = 0;
            toggle_mode();
            // Do not process these keycodes any further
            return false;
        }
    } else {
        // This key was released, remove it from the bitflag set
        fs_switch_mask &= ~key;
    }
    // Continue processing
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    bool pressed = record->event.pressed;

    switch (keycode) {
        case FS_DQT_SQH: {
            if (handle_layer_switch_combo(FS_DQ_PRESS, pressed)) {
                perform_tap_hold(pressed, KC_DQUO, KC_QUOT, keycode);
            }
            return false;
        }

        case FS_OPT_CPH: {
            if (handle_layer_switch_combo(FS_OP_PRESS, pressed)) {
                perform_tap_hold(pressed, KC_LPRN, KC_RPRN, keycode);
            }
            return false;
        }

        case FS_EMT_SMH: {
            if (handle_layer_switch_combo(FS_EM_PRESS, pressed)) {
                perform_tap_hold(pressed, KC_EXLM, KC_SCLN, keycode);
            }
            return false;
        }

        case FS_QMT_CLH: {
            if (handle_layer_switch_combo(FS_QM_PRESS, pressed)) {
                perform_tap_hold(pressed, KC_QUES, KC_COLN, keycode);
            }
            return false;
        }

        case FS_PDT_CMH: {
            perform_tap_hold(pressed, KC_DOT, KC_COMM, keycode);
            return false;
        }

        default: {
            return true;
        }
    }
}
