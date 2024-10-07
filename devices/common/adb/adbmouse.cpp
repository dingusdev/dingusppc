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

/** @file Apple Desktop Bus Mouse emulation. */

#include <devices/common/adb/adbmouse.h>
#include <devices/common/adb/adbbus.h>
#include <devices/deviceregistry.h>
#include <core/hostevents.h>
#include <memaccess.h>
#include <loguru.hpp>

AdbMouse::AdbMouse(
    std::string name, uint8_t device_class, int num_buttons, int num_bits, uint16_t resolution) : AdbDevice(name), device_class(device_class), num_buttons(num_buttons), num_bits(num_bits), resolution(resolution) {
    EventManager::get_instance()->add_mouse_handler(this, &AdbMouse::event_handler);

    this->reset();
}

void AdbMouse::event_handler(const MouseEvent& event) {
    if (event.flags & MOUSE_EVENT_MOTION) {
        this->x_rel += event.xrel;
        this->y_rel += event.yrel;
        this->x_abs = event.xabs;
        this->y_abs = event.yabs;
        if (this->device_class == TABLET)
            this->changed = true;
    } else if (event.flags & MOUSE_EVENT_BUTTON) {
        this->buttons_state = event.buttons_state & ((1 << this->num_buttons) - 1);
        this->x_abs = event.xabs;
        this->y_abs = event.yabs;
        this->changed = true;
    }
}

void AdbMouse::reset() {
    this->my_addr = ADB_ADDR_RELPOS;
    this->dev_handler_id = 1;
    this->exc_event_flag = 1;
    this->srq_flag = 1; // enable service requests
    this->x_rel = 0;
    this->y_rel = 0;
    this->x_abs = 0;
    this->y_abs = 0;
    this->buttons_state = 0;
    this->changed = false;
}

bool AdbMouse::get_register_0(uint8_t buttons_state, bool force) {
    bool should_update = this->x_rel || this->y_rel || this->changed || force;

    if (should_update) {
        uint8_t* p;
        uint8_t* out_buf = this->host_obj->get_output_buf();

        static const uint8_t buttons_to_bits[] = {0, 7, 7, 10, 10, 13, 13, 16, 16};
        static const uint8_t bits_to_bits[]    = {0, 7, 7, 7, 7, 7, 7, 7, 10, 10, 10, 13, 13, 13, 16, 16, 16};
        int total_bits = std::max(buttons_to_bits[this->num_buttons], bits_to_bits[this->num_bits]);

        for (int axis = 0; axis < 2; axis++) {
            int bits = this->num_bits;
            int32_t val = axis ? this->device_class == TABLET ? this->x_abs : this->x_rel
                               : this->device_class == TABLET ? this->y_abs : this->y_rel;
            if (val < (-1 << (bits - 1)))
                val = -1 << (bits - 1);
            else if (val >= (1 << (bits - 1)))
                val = (1 << (bits - 1)) - 1;
            int bits_remaining = total_bits;
            p = &out_buf[axis];
            int button = axis;
            bits = 7;

            while (bits_remaining > 0) {
                *p = (val & ((1 << bits) - 1)) | (((buttons_state >> button) ^ 1) << bits) | (*p << (bits + 1));
                val >>= bits;
                bits_remaining -= bits;
                p = bits == 7 ? &out_buf[2] : p + 1;
                button += 2;
                bits = 3;
            }
        }

        // reset accumulated motion data and button change flag
        this->x_rel     = 0;
        this->y_rel     = 0;
        this->changed   = false;

        uint8_t count = (uint8_t)(p - out_buf);
        // should never happen, but check just in case
        if (((size_t)p - (size_t)out_buf) > UINT8_MAX)
            count = UINT8_MAX;
        // if the mouse is in standard protocol then only send first 2 bytes
        // BUGBUG: what should tablet do here?
        if (this->device_class == MOUSE && this->dev_handler_id == 1)
            count = 2;
        this->host_obj->set_output_count(count);
        return should_update;
    }

    return false;
}

bool AdbMouse::get_register_0() {
    return get_register_0(this->buttons_state, false);
}

bool AdbMouse::get_register_1() {
    uint8_t* out_buf = this->host_obj->get_output_buf();
    // Identifier
    out_buf[0] = 'a';
    out_buf[1] = 'p';
    out_buf[2] = 'p';
    out_buf[3] = 'l';
    // Slightly higher resolution of 300 units per inch
    WRITE_WORD_BE_A(&out_buf[4], resolution);
    out_buf[6] = device_class;
    out_buf[7] = num_buttons;
    this->host_obj->set_output_count(8);
    return true;
}

void AdbMouse::set_register_3() {
    if (this->host_obj->get_input_count() < 2) // ensure we got enough data
        return;

    const uint8_t*  in_data = this->host_obj->get_input_buf();

    switch (in_data[1]) {
    case 0:
        this->my_addr  = in_data[0] & 0xF;
        this->srq_flag = !!(in_data[0] & 0x20);
        break;
    case 1:
    case 2:
    case 4: // switch over to extended mouse protocol
        this->dev_handler_id = in_data[1];
        break;
    case 0xFE: // move to a new address if there was no collision
        if (!this->got_collision) {
            this->my_addr = in_data[0] & 0xF;
        }
        break;
    default:
        LOG_F(WARNING, "%s: unknown handler ID = 0x%X", this->name.c_str(), in_data[1]);
    }
}

uint8_t AdbMouse::get_buttons_state() const {
    return this->buttons_state;
}

static const DeviceDescription AdbMouse_Descriptor = {
    AdbMouse::create, {}, {}
};

REGISTER_DEVICE(AdbMouse, AdbMouse_Descriptor);
