/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

/** @file Enhanced Serial Communications Controller (ESCC) emulation. */

#include "escc.h"
#include <loguru.hpp>

#include <cinttypes>
#include <memory>

EsccController::EsccController()
{
    this->ch_a = std::unique_ptr<EsccChannel> (new EsccChannel("A"));
    this->ch_b = std::unique_ptr<EsccChannel> (new EsccChannel("B"));

    this->reg_ptr = 0;
}

uint8_t EsccController::read(uint8_t reg_offset)
{
    switch(reg_offset) {
    case EsccReg::Port_B_Cmd:
        LOG_F(9, "ESCC: reading Port B register RR%d", this->reg_ptr);
        this->reg_ptr = 0;
        break;
    case EsccReg::Port_A_Cmd:
        LOG_F(9, "ESCC: reading Port A register RR%d", this->reg_ptr);
        this->reg_ptr = 0;
        break;
    default:
        LOG_F(INFO, "ESCC: reading register %d", reg_offset);
    }

    return 0;
}

void EsccController::write(uint8_t reg_offset, uint8_t value)
{
    switch(reg_offset) {
    case EsccReg::Port_B_Cmd:
        this->write_internal(this->ch_b.get(), value);
        break;
    case EsccReg::Port_A_Cmd:
        this->write_internal(this->ch_a.get(), value);
        break;
    default:
        LOG_F(INFO, "ESCC: writing 0x%X to register %d", value, reg_offset);
    }
}

uint8_t EsccController::read_compat(uint8_t reg_offset)
{
    switch(reg_offset) {
    case Compat::EsccReg::Port_B_Cmd:
        LOG_F(9, "ESCC: reading Port B register RR%d", this->reg_ptr);
        this->reg_ptr = 0;
        break;
    case Compat::EsccReg::Port_A_Cmd:
        LOG_F(9, "ESCC: reading Port A register RR%d", this->reg_ptr);
        this->reg_ptr = 0;
        break;
    default:
        LOG_F(INFO, "ESCC: reading register %d", reg_offset);
    }

    return 0;
}

void EsccController::write_compat(uint8_t reg_offset, uint8_t value)
{
    switch(reg_offset) {
    case Compat::EsccReg::Port_B_Cmd:
        this->write_internal(this->ch_b.get(), value);
        break;
    case Compat::EsccReg::Port_A_Cmd:
        this->write_internal(this->ch_a.get(), value);
        break;
    default:
        LOG_F(INFO, "ESCC: writing 0x%X to register %d", value, reg_offset);
    }
}

void EsccController::write_internal(EsccChannel *ch, uint8_t value)
{
    if (this->reg_ptr) {
        ch->write_reg(this->reg_ptr, value);
        this->reg_ptr = 0;
    } else {
        this->reg_ptr = value & 7;
        switch(value >> 3) {
        case WR0Cmd::Point_High:
            this->reg_ptr |= 8;
            break;
        }
    }
}

void EsccChannel::write_reg(int reg_num, uint8_t value)
{
    this->write_regs[reg_num] = value;
    LOG_F(9, "ESCC: writing 0x%X to Channel %s WR%d", value, this->name.c_str(),
          reg_num);
}
