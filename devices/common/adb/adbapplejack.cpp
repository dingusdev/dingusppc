/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

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

/** @file Apple Desktop Bus AppleJack controller emulation. */

#include <devices/common/adb/adbapplejack.h>
#include <devices/common/adb/adbbus.h>
#include <devices/deviceregistry.h>
#include <core/hostevents.h>
#include <loguru.hpp>

AdbAppleJack::AdbAppleJack(std::string name) : AdbMouse(name, AdbMouse::TRACKBALL, 2, 7, 300) {
    EventManager::get_instance()->add_gamepad_handler(this, &AdbAppleJack::event_handler);
}

void AdbAppleJack::event_handler(const GamepadEvent& event) {
    uint32_t button_bit = 1 << event.button;

    if (event.flags & GAMEPAD_EVENT_DOWN)
        this->buttons_state |= button_bit;
    else if (event.flags & GAMEPAD_EVENT_UP)
        this->buttons_state &= ~button_bit;

    if (button_bit & ((1 << GamepadButton::LeftTrigger) | (1 << GamepadButton::RightTrigger)))
        this->triggers_changed = true;
    else
        this->buttons_changed = true;
}

void AdbAppleJack::reset() {
    this->AdbMouse::reset();
    this->buttons_state = 0;
    this->triggers_changed = false;
    this->buttons_changed = false;
    LOG_F(INFO, "%s: reset; in mouse emulation mode", this->name.c_str());
}

bool AdbAppleJack::get_register_0() {
    bool changed = this->triggers_changed || this->buttons_changed;

    uint8_t mouse_buttons = this->AdbMouse::get_buttons_state();
    // AppleJack triggers always affect the mouse button bits.
    mouse_buttons |= (this->buttons_state >> (GamepadButton::LeftTrigger  - 0)) & 1;
    mouse_buttons |= (this->buttons_state >> (GamepadButton::RightTrigger - 1)) & 2;
    changed |= this->AdbMouse::get_register_0(mouse_buttons, changed);
    this->triggers_changed = false;

    if (this->dev_handler_id == APPLEJACK_HANDLER_ID && this->buttons_changed) {
        uint8_t* out_buf = this->host_obj->get_output_buf();

        out_buf[2] = ~this->buttons_state >> 8;
        out_buf[3] = ~this->buttons_state & 0xFF;

        this->host_obj->set_output_count(4);
        this->buttons_changed = false;
    }

    return changed;
}

void AdbAppleJack::set_register_3() {
    if (this->host_obj->get_input_count() < 2) // ensure we got enough data
        return;

    const uint8_t* in_data = this->host_obj->get_input_buf();

    switch (in_data[1]) {
    case APPLEJACK_HANDLER_ID: // switch over to AppleJack protocol
        this->dev_handler_id = in_data[1];
        LOG_F(INFO, "%s: switched to AppleJack mode", this->name.c_str());
        break;
    default:
        this->AdbMouse::set_register_3();
        break;
    }
}

static const DeviceDescription AdbAppleJack_Descriptor = {
    AdbAppleJack::create, {}, {}
};

REGISTER_DEVICE(AdbAppleJack, AdbAppleJack_Descriptor);
