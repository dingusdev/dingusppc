/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** @file Apple Desktop Bus Keyboard definitions. */

#ifndef ADB_KEYBOARD_H
#define ADB_KEYBOARD_H

#include <devices/common/adb/adbdevice.h>
#include <devices/common/hwcomponent.h>

#include <deque>
#include <memory>
#include <string>

class KeyboardEvent;

class AdbKeyboard : public AdbDevice {
public:
    AdbKeyboard(std::string name);
    ~AdbKeyboard() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AdbKeyboard>(new AdbKeyboard("ADB-KEYBOARD"));
    }

    void reset() override;
    void event_handler(const KeyboardEvent& event);

    bool get_register_0() override;
    bool get_register_2() override;
    bool get_register_3() override;
    void set_register_2() override;
    void set_register_3() override;


private:
    std::deque<std::unique_ptr<KeyboardEvent>> pending_events;

    uint8_t consume_pending_event();
};

// ADB Extended Keyboard raw key codes (most of which eventually became virtual
// key codes, see HIToolbox/Events.h).
enum AdbKey {
    AdbKey_A =              0x00,
    AdbKey_B =              0x0b,
    AdbKey_C =              0x08,
    AdbKey_D =              0x02,
    AdbKey_E =              0x0e,
    AdbKey_F =              0x03,
    AdbKey_G =              0x05,
    AdbKey_H =              0x04,
    AdbKey_I =              0x22,
    AdbKey_J =              0x26,
    AdbKey_K =              0x28,
    AdbKey_L =              0x25,
    AdbKey_M =              0x2e,
    AdbKey_N =              0x2d,
    AdbKey_O =              0x1f,
    AdbKey_P =              0x23,
    AdbKey_Q =              0x0c,
    AdbKey_R =              0x0f,
    AdbKey_S =              0x01,
    AdbKey_T =              0x11,
    AdbKey_U =              0x20,
    AdbKey_V =              0x09,
    AdbKey_W =              0x0d,
    AdbKey_X =              0x07,
    AdbKey_Y =              0x10,
    AdbKey_Z =              0x06,

    AdbKey_1 =              0x12,
    AdbKey_2 =              0x13,
    AdbKey_3 =              0x14,
    AdbKey_4 =              0x15,
    AdbKey_5 =              0x17,
    AdbKey_6 =              0x16,
    AdbKey_7 =              0x1a,
    AdbKey_8 =              0x1c,
    AdbKey_9 =              0x19,
    AdbKey_0 =              0x1d,

    AdbKey_Minus =          0x1b,
    AdbKey_Equal =          0x18,
    AdbKey_LeftBracket =    0x21,
    AdbKey_RightBracket =   0x1e,
    AdbKey_Backslash =      0x2a,
    AdbKey_Semicolon =      0x29,
    AdbKey_Quote =          0x27,
    AdbKey_Comma =          0x2b,
    AdbKey_Period =         0x2f,
    AdbKey_Slash =          0x2c,

    AdbKey_Tab =            0x30,
    AdbKey_Return =         0x24,
    AdbKey_Space =          0x31,
    AdbKey_Delete =         0x33,

    AdbKey_ForwardDelete =  0x75,
    AdbKey_Help =           0x72,
    AdbKey_Home =           0x73,
    AdbKey_End =            0x77,
    AdbKey_PageUp =         0x74,
    AdbKey_PageDown =       0x79,

    AdbKey_Grave =          0x32,
    AdbKey_Escape =         0x35,
    AdbKey_Control =        0x36,
    AdbKey_Shift =          0x38,
    AdbKey_Option =         0x3a,
    AdbKey_Command =        0x37,
    AdbKey_CapsLock =       0x39,

    AdbKey_RightControl   = 0x7d,
    AdbKey_RightShift     = 0x7b,
    AdbKey_RightOption    = 0x7c,

    AdbKey_ArrowUp =        0x3e,
    AdbKey_ArrowDown =      0x3d,
    AdbKey_ArrowLeft =      0x3b,
    AdbKey_ArrowRight =     0x3c,

    AdbKey_Keypad0 =        0x52,
    AdbKey_Keypad1 =        0x53,
    AdbKey_Keypad2 =        0x54,
    AdbKey_Keypad3 =        0x55,
    AdbKey_Keypad4 =        0x56,
    AdbKey_Keypad5 =        0x57,
    AdbKey_Keypad6 =        0x58,
    AdbKey_Keypad7 =        0x59,
    AdbKey_Keypad9 =        0x5c,
    AdbKey_Keypad8 =        0x5b,
    AdbKey_KeypadDecimal =  0x41,
    AdbKey_KeypadPlus =     0x45,
    AdbKey_KeypadMinus =    0x4e,
    AdbKey_KeypadMultiply = 0x43,
    AdbKey_KeypadDivide =   0x4b,
    AdbKey_KeypadEnter =    0x4c,
    AdbKey_KeypadEquals =   0x51,
    AdbKey_KeypadClear =    0x47,

    AdbKey_F1 =             0x7a,
    AdbKey_F2 =             0x78,
    AdbKey_F3 =             0x63,
    AdbKey_F4 =             0x76,
    AdbKey_F5 =             0x60,
    AdbKey_F6 =             0x61,
    AdbKey_F7 =             0x62,
    AdbKey_F8 =             0x64,
    AdbKey_F9 =             0x65,
    AdbKey_F10 =            0x6d,
    AdbKey_F11 =            0x67,
    AdbKey_F12 =            0x6f,
    AdbKey_F13 =            0x69,
    AdbKey_F14 =            0x6b,
    AdbKey_F15 =            0x71,

    AdbKey_ISO1=            0x0A,

    AdbKey_JIS_Yen        = 0x5D,
    AdbKey_JIS_Underscore = 0x5E,
    AdbKey_JIS_KP_Comma   = 0x5F,
    AdbKey_JIS_Eisu       = 0x66,
    AdbKey_JIS_Kana       = 0x68,
};

#endif    // ADB_KEYBOARD_H
