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

/** Houdini2 PC Compatibility card emulation. */

#include <devices/deviceregistry.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/nubus/houdini2.h>
#include <devices/common/nubus/nubusutils.h>
#include <devices/memctrl/memctrlbase.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>

HoudiniCard::HoudiniCard()
{
    supports_types(HWCompType::PDS_DEV);

    this->pl_obj = std::unique_ptr<PretzelLogic> (new PretzelLogic());

    auto mem_crtl = dynamic_cast<MemCtrlBase*>(gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    mem_crtl->add_mmio_region(0xE0000000, 0x10000, this->pl_obj.get());

    // load card's declaration ROM
    if (load_declaration_rom("Houdini2_DeclROM.bin", 0xE)) {
        LOG_F(ERROR, "Could not load declaration ROM, the card won't work!");
    }
}

PretzelLogic::PretzelLogic()
{
    supports_types(HWCompType::MMIO_DEV);

    // tell the world we got some own memory
    // (shared memory will be disabled)
    this->air3 = Air3Bits::SIMM_INSTALLED;

    this->air_reset = 1; // tell the world our PC is not currently running
}

uint32_t PretzelLogic::read(uint32_t rgn_start, uint32_t offset, int size)
{
    switch (offset) {
    case PretzelLogicReg::AIR_RESET:
        return this->air_reset;
    case PretzelLogicReg::AIR3:
        return this->air3;
    default:
        LOG_F(WARNING, "PretzelLogic: reading from unimplemented reg at 0x%X", offset);
    }

    return 0xFF;
}

void PretzelLogic::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    switch (offset) {
    case PretzelLogicReg::MAIN_IER:
        this->main_ier = value;
        if (!value) {
            LOG_F(INFO, "PretzelLogic: all interrupts disabled");
        }
        break;
    default:
        LOG_F(WARNING, "PretzelLogic: writing to unimplemented reg at 0x%X", offset);
    }
}

static const DeviceDescription Houdini_Descriptor = {
    HoudiniCard::create, {}, {}
};

REGISTER_DEVICE(Houdini2, Houdini_Descriptor);
