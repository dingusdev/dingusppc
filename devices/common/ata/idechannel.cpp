/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

/** IDE Channel (aka IDE port) emulation.

    One IDE channel is capable of controlling up to two IDE devices.
    This class handles device registration and passing of messages
    from and to the host.
 */

#include <devices/common/ata/atadefs.h>
#include <devices/common/ata/idechannel.h>
#include <devices/common/hwcomponent.h>
#include <devices/deviceregistry.h>

#include <cinttypes>
#include <memory>
#include <string>

IdeChannel::IdeChannel(const std::string name)
{
    this->set_name(name);
    this->supports_types(HWCompType::IDE_BUS);

    this->devices[0] = std::unique_ptr<AtaNullDevice>(new AtaNullDevice());
    this->devices[1] = std::unique_ptr<AtaNullDevice>(new AtaNullDevice());
}

uint16_t IdeChannel::read(const uint8_t reg_addr, const int size)
{
    return this->devices[this->cur_dev]->read(reg_addr);
}

void IdeChannel::write(const uint8_t reg_addr, const uint16_t val, const int size)
{
    // keep track of the currently selected device
    if (reg_addr == IDE_Reg::DEVICE_HEAD) {
        this->cur_dev = (val >> 4) & 1;
    }

    // redirect register writes to both devices
    for (auto& dev : this->devices) {
        dev->write(reg_addr, val);
    }
}

static const DeviceDescription Ide0_Descriptor = {
    IdeChannel::create_first, {}, {}
};

static const DeviceDescription Ide1_Descriptor = {
    IdeChannel::create_second, {}, {}
};

REGISTER_DEVICE(Ide0, Ide0_Descriptor);
REGISTER_DEVICE(Ide1, Ide1_Descriptor);
