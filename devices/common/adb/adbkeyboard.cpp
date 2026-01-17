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
    this->pending_events.push_back(std::make_unique<KeyboardEvent>(event));

    if (this->pending_events.size() == 1) {
        this->srq_flag = 1;
    }
}

void AdbKeyboard::reset() {
    this->my_addr        = ADB_ADDR_KBD;
    this->dev_handler_id = 2;    // Extended ADB keyboard
    this->exc_event_flag = 1;
    this->srq_flag       = 0;    // don't process keyboard service requests yet
    this->led_state      = 0;    // LEDs off
    this->pending_events.clear();
}

bool AdbKeyboard::get_register_0() {
    if (this->pending_events.empty()) {
        return false;
    }
    uint8_t* out_buf = this->host_obj->get_output_buf();
    out_buf[0] = this->consume_pending_event();
    out_buf[1] = this->consume_pending_event();
    this->host_obj->set_output_count(2);

    if (this->pending_events.empty()) {
        this->srq_flag = 0;
    }

    return true;
}

uint8_t AdbKeyboard::consume_pending_event() {
    if (this->pending_events.empty()) {
        // In most cases we have only one pending event when the host polls us,
        // but we need to fill two entries of the output buffer. We need to set
        // the key status bit to 1 (released), and the key to a non-existent
        // one (0x7F). Otherwise if we leave it empty, the host will think that
        // the 'a' key (code 0) is pressed (status 0).
        return 0xFF;
    }
    std::unique_ptr<KeyboardEvent> event = std::move(this->pending_events.front());
    this->pending_events.pop_front();

    uint8_t key_state = (event->flags & KEYBOARD_EVENT_UP) ? 1 : 0;

    if (this->dev_handler_id != 3) {
        switch (event->key) {
            case AdbKey_RightControl : event->key = AdbKey_Control; break;
            case AdbKey_RightShift   : event->key = AdbKey_Shift  ; break;
            case AdbKey_RightOption  : event->key = AdbKey_Option ; break;
        }
    }

    return (key_state << 7) | (event->key & 0x7F);
}

bool AdbKeyboard::get_register_2() {
    uint8_t* out_buf = this->host_obj->get_output_buf();
    out_buf[0]       = 0;
    out_buf[1]       = this->led_state & 0x07;
    this->host_obj->set_output_count(2);
    return true;
}

void AdbKeyboard::set_register_2() {
    if (this->host_obj->get_input_count() < 2)
        return;

    const uint8_t* in_data = this->host_obj->get_input_buf();

    this->led_state        = in_data[1] & 0x07;
}

bool AdbKeyboard::get_register_3() {
    uint8_t* out_buf = this->host_obj->get_output_buf();

    out_buf[0] = (this->my_addr << 4) | (1 << 1) | 1;    // Address + SRQ Enable + Exc Event
    out_buf[1] = this->dev_handler_id;

    this->host_obj->set_output_count(2);
    return true;
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
    AdbKeyboard::create, {}, {}, HWCompType::ADB_DEV
};

REGISTER_DEVICE(AdbKeyboard, AdbKeyboard_Descriptor);
