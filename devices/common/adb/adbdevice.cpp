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

/** @file Base class for Apple Desktop Bus devices. */

#include <core/timermanager.h>
#include <devices/common/adb/adbdevice.h>
#include <devices/common/adb/adbbus.h>
#include <machines/machinebase.h>

AdbDevice::AdbDevice(std::string name) {
    this->set_name(name);
    this->supports_types(HWCompType::ADB_DEV);
}

int AdbDevice::device_postinit() {
    // register itself with the ADB host
    this->host_obj = dynamic_cast<AdbBus*>(gMachineObj->get_comp_by_type(HWCompType::ADB_HOST));
    this->host_obj->register_device(this);

    return 0;
};

uint8_t AdbDevice::poll() {
    if (!this->srq_flag) {
        return 0;
    }
    bool has_data = this->get_register_0();
    if (!has_data) {
        return 0;
    }

    // Register 0 in bits 0-1 (both 0)
    // Talk command in bits 2-3 (both 1)
    // Device address in bits 4-7
    return 0xC | (this->my_addr << 4);
}

bool AdbDevice::talk(const uint8_t dev_addr, const uint8_t reg_num) {
    if (dev_addr == this->my_addr && !this->got_collision) {
        // see if another device already responded to this command
        if (this->host_obj->already_answered()) {
            this->got_collision = true;
            return false;
        }

        switch(reg_num & 3) {
        case 0:
            return this->get_register_0();
        case 1:
            return this->get_register_1();
        case 2:
            return this->get_register_2();
        case 3:
            return this->get_register_3();
        default:
            return false;
        }
    } else {
        return false;
    }
}

void AdbDevice::listen(const uint8_t dev_addr, const uint8_t reg_num) {
    if (dev_addr == this->my_addr) {
        switch(reg_num & 3) {
        case 0:
            this->set_register_0();
        case 1:
            this->set_register_1();
        case 2:
            this->set_register_2();
        case 3:
            this->set_register_3();
        }
    }
}

bool AdbDevice::get_register_3() {
    uint8_t* out_buf = this->host_obj->get_output_buf();
    out_buf[0] = this->gen_random_address() | (this->exc_event_flag << 6) |
                 (this->srq_flag << 5);
    out_buf[1] = this->dev_handler_id;
    this->host_obj->set_output_count(2);
    return true;
}

uint8_t AdbDevice::gen_random_address() {
    return (TimerManager::get_instance()->current_time_ns() + 8) & 0xF;
}
