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

/** @file Media Access Controller for Ethernet (MACE) emulation. */

#include <devices/deviceregistry.h>
#include <devices/ethernet/mace.h>
#include <loguru.hpp>

#include <cinttypes>

using namespace MaceEnet;

uint8_t MaceController::read(uint8_t reg_offset)
{
    switch(reg_offset) {
    case MaceReg::Rcv_Frame_Ctrl:
        return this->rcv_fc;
    case MaceReg::Interrupt: {
        uint8_t ret_val = this->int_stat;
        this->int_stat = 0;
        LOG_F(9, "%s: all interrupt flags cleared", this->name.c_str());
        return ret_val;
    }
    case MaceReg::Interrupt_Mask:
        return this->int_mask;
    case MaceReg::BIU_Config_Ctrl:
        return this->biu_ctrl;
    case MaceReg::Chip_ID_Lo:
        return this->chip_id & 0xFFU;
    case MaceReg::Chip_ID_Hi:
        return (this->chip_id >> 8) & 0xFFU;
    default:
        LOG_F(INFO, "%s: reading from register %d", this->name.c_str(), reg_offset);
    }

    return 0;
}

void MaceController::write(uint8_t reg_offset, uint8_t value)
{
    switch(reg_offset) {
    case MaceReg::Rcv_Frame_Ctrl:
        this->rcv_fc = value;
        break;
    case MaceReg::Interrupt_Mask:
        this->int_mask = value;
        break;
    case MaceReg::MAC_Config_Ctrl:
        this->mac_cfg = value;
        break;
    case MaceReg::BIU_Config_Ctrl:
        if (value & BIU_SWRST) {
            LOG_F(INFO, "%s: soft reset asserted", this->name.c_str());
            value &= ~BIU_SWRST; // acknowledge soft reset
        }
        this->biu_ctrl = value;
        break;
    case MaceReg::PLS_Config_Ctrl:
        if (value != 7)
            LOG_F(WARNING, "%s: unsupported transceiver interface 0x%X in PLSCC",
                  this->name.c_str(), value);
        break;
    case MaceReg::Int_Addr_Config:
        if ((value & IAC_LOGADDR) && (value & IAC_PHYADDR))
            value &= ~IAC_PHYADDR;
        if (value & (IAC_LOGADDR | IAC_PHYADDR))
            this->addr_ptr = 0;
        this->addr_cfg = value;
        break;
    case MaceReg::Log_Addr_Flt:
        if (this->addr_cfg & IAC_LOGADDR) {
            uint64_t mask = ~(0xFFULL << (this->addr_ptr * 8));
            this->log_addr = (this->log_addr & mask) | ((uint64_t)value << (this->addr_ptr * 8));
            if (++this->addr_ptr >= 8) {
                this->addr_cfg &= ~IAC_LOGADDR;
                this->addr_ptr = 0;
            }
        }
        break;
    case MaceReg::Phys_Addr:
        if (this->addr_cfg & IAC_PHYADDR) {
            uint64_t mask = ~(0xFFULL << (this->addr_ptr * 8));
            this->phys_addr = (this->phys_addr & mask) | ((uint64_t)value << (this->addr_ptr * 8));
            if (++this->addr_ptr >= 6) {
                this->addr_cfg &= ~IAC_PHYADDR;
                this->addr_ptr = 0;
            }
        }
        break;
    default:
        LOG_F(INFO, "%s: writing 0x%X to register %d", this->name.c_str(),
              value, reg_offset);
    }
}

static const DeviceDescription Mace_Descriptor = {
    MaceController::create, {}, {}
};

REGISTER_DEVICE(Mace, Mace_Descriptor);
