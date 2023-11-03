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

/** @file Apple Desktop Bus emulation. */

#include <devices/common/adb/adbbus.h>
#include <devices/common/adb/adbdevice.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>

AdbBus::AdbBus(std::string name) {
    this->set_name(name);
    supports_types(HWCompType::ADB_HOST);
    this->devices.clear();
}

void AdbBus::register_device(AdbDevice* dev_obj) {
    this->devices.push_back(dev_obj);
}

uint8_t AdbBus::process_command(const uint8_t* in_data, int data_size) {
    uint8_t dev_addr, dev_reg;

    this->output_count  = 0;

    if (!data_size)
        return ADB_STAT_OK;

    uint8_t cmd_byte = in_data[0];
    uint8_t cmd = cmd_byte & 0xF;

    if(!cmd) { // SendReset
        LOG_F(9, "%s: SendReset issued", this->name.c_str());
        for (auto dev : this->devices)
            dev->reset();
    } else if (cmd == 1) { // Flush
        dev_addr = cmd_byte >> 4;

        LOG_F(9, "%s: Flush issued, dev_addr=0x%X", this->name.c_str(), dev_addr);
    } else if ((cmd & 0xC) == 8) { // Listen
        dev_addr = cmd_byte >> 4;
        dev_reg  = cmd_byte & 3;

        LOG_F(9, "%s: Listen R%d issued, dev_addr=0x%X", this->name.c_str(),
              dev_reg, dev_addr);

        this->input_buf   = in_data + 1;
        this->input_count = data_size - 1;

        for (auto dev : this->devices)
            dev->listen(dev_addr, dev_reg);
    } else if ((cmd & 0xC) == 0xC) { // Talk
        dev_addr = cmd_byte >> 4;
        dev_reg  = cmd_byte & 3;

        LOG_F(9, "%s: Talk R%d issued, dev_addr=0x%X", this->name.c_str(),
              dev_reg, dev_addr);

        this->got_answer = false;

        for (auto dev : this->devices)
            this->got_answer = dev->talk(dev_addr, dev_reg);

        if (!this->got_answer)
            return ADB_STAT_TIMEOUT;
    } else {
        ABORT_F("%s: unsupported ADB command 0x%X", this->name.c_str(), cmd_byte);
    }

    return ADB_STAT_OK;
}

static const DeviceDescription AdbBus_Descriptor = {
    AdbBus::create, {}, {}
};

REGISTER_DEVICE(AdbBus, AdbBus_Descriptor);
