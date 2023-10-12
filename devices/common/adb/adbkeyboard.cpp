/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** @file Apple Desktop Bus Keyboard emulation. */

#include <devices/common/adb/adbkeyboard.h>
#include <devices/common/adb/adbbus.h>
#include <devices/deviceregistry.h>
#include <core/hostevents.h>
#include <loguru.hpp>

AdbKeyboard::AdbKeyboard(std::string name) : AdbDevice(name) {
    EventManager::get_instance()->add_keyboard_handler(this, &AdbKeyboard::event_handler);

    this->reset();
}

void AdbKeyboard::event_handler(const KeyboardEvent& event) {
    this->key = event.key;
    if (event.flags & KEYBOARD_EVENT_DOWN) {
        this->key_state = 0;
    }
    else if (event.flags & KEYBOARD_EVENT_UP) {
        this->key_state = 1;
    }
    this->changed = true;
}

void AdbKeyboard::reset() {
    this->my_addr        = ADB_ADDR_KBD;
    this->dev_handler_id = 2;    // Extended ADB keyboard
    this->exc_event_flag = 1;
    this->srq_flag       = 1;    // enable service requests
    this->key            = 0;
    this->key_state      = 0;
    this->changed        = false;
}

bool AdbKeyboard::get_register_0() {
    if (this->changed) {
        uint8_t* out_buf = this->host_obj->get_output_buf();

        out_buf[0] = (this->key_state << 7) | (this->key & 0x7F);
        // It's possible that we get two events before the host polls us, but
        // in practice it has not come up. We need to set the key status bit to
        // 1 (released), otherwise if we leave it empty, the host will think
        // that the a key (code 0) is pressed.
        out_buf[1] = 1 << 7;

        this->key           = 0;
        this->key_state     = 0;
        this->changed       = false;

        this->host_obj->set_output_count(2);
        return true;
    }

    return false;
}

void AdbKeyboard::set_register_2() {
}

void AdbKeyboard::set_register_3() {
    if (this->host_obj->get_input_count() < 2)    // ensure we got enough data
        return;

    const uint8_t* in_data = this->host_obj->get_input_buf();

    switch (in_data[1]) {
    case 0:
        this->my_addr  = in_data[0] & 0xF;
        this->srq_flag = !!(in_data[0] & 0x20);
        break;
    case 1:
    case 2:
        this->dev_handler_id = in_data[1];
        break;
    case 3:    // extended keyboard protocol isn't supported yet
        break;
    case 0xFE:    // move to a new address if there was no collision
        if (!this->got_collision) {
            this->my_addr = in_data[0] & 0xF;
        }
        break;
    default:
        LOG_F(WARNING, "%s: unknown handler ID = 0x%X", this->name.c_str(), in_data[1]);
    }
}

static const DeviceDescription AdbKeyboard_Descriptor = {
    AdbKeyboard::create, {}, {}
};

REGISTER_DEVICE(AdbKeyboard, AdbKeyboard_Descriptor);
