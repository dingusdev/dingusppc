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

/** IDE Channel (aka IDE port) emulation.

    One IDE channel is capable of controlling up to two IDE devices.

    IdeChannel class handles device registration and passing of messages
    from and to the host.

    MacioIdeChannel class implements MacIO specific registers
    and interrupt handling.
 */

#include <devices/common/ata/atabasedevice.h>
#include <devices/common/ata/atadefs.h>
#include <devices/common/ata/idechannel.h>
#include <devices/common/hwcomponent.h>
#include <devices/deviceregistry.h>
#include <machines/machinebase.h>
#include <loguru.hpp>

#include <cinttypes>
#include <memory>
#include <string>

using namespace ata_interface;

IdeChannel::IdeChannel(const std::string name)
{
    this->set_name(name);
    this->supports_types(HWCompType::IDE_BUS);

    this->device_stub = std::unique_ptr<AtaNullDevice>(new AtaNullDevice());

    this->devices[0] = this->device_stub.get();
    this->devices[1] = this->device_stub.get();
}

void IdeChannel::register_device(int id, AtaInterface* dev_obj) {
    if (id < 0 || id >= 2)
        ABORT_F("%s: invalid device ID", this->name.c_str());

    this->devices[id] = dev_obj;

    ((AtaBaseDevice*)dev_obj)->set_host(this, id);
}

uint32_t IdeChannel::read(const uint8_t reg_addr, const int size) {
    return this->devices[this->cur_dev]->read(reg_addr);
}

void IdeChannel::write(const uint8_t reg_addr, const uint32_t val, const int size)
{
    // keep track of the currently selected device
    if (reg_addr == DEVICE_HEAD) {
        this->cur_dev = (val >> 4) & 1;
    }

    // redirect register writes to both devices
    for (auto& dev : this->devices) {
        dev->write(reg_addr, val);
    }
}

int MacioIdeChannel::device_postinit() {
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(
        this->name == "IDE0" ? IntSrc::IDE0 : IntSrc::IDE1);

    this->irq_callback = [this](const uint8_t intrq_state) {
        this->int_ctrl->ack_int(this->irq_id, intrq_state);
    };

    return 0;
}

uint32_t MacioIdeChannel::read(const uint8_t reg_addr, const int size)
{
    if (reg_addr == TIME_CONFIG) {
        if (size != 4) {
            LOG_F(WARNING, "%s: non-DWORD read from TIME_CONFIG", this->name.c_str());
        }
        return this->ch_config;
    } else
        return IdeChannel::read(reg_addr, size);
}

void MacioIdeChannel::write(const uint8_t reg_addr, const uint32_t val, const int size)
{
    if (reg_addr == TIME_CONFIG) {
        if (size != 4) {
            LOG_F(WARNING, "%s: non-DWORD write to TIME_CONFIG", this->name.c_str());
        }
        this->ch_config = val;
    } else
        IdeChannel::write(reg_addr, val, size);
}

static const DeviceDescription Ide0_Descriptor = {
    MacioIdeChannel::create_first, {}, {}, HWCompType::IDE_BUS
};

static const DeviceDescription Ide1_Descriptor = {
    MacioIdeChannel::create_second, {}, {}, HWCompType::IDE_BUS
};

REGISTER_DEVICE(Ide0, Ide0_Descriptor);
REGISTER_DEVICE(Ide1, Ide1_Descriptor);
