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

/** @file Sander-Wozniak Machine 3 (SWIM3) emulation. */

#include "superdrive.h"
#include "swim3.h"
#include <loguru.hpp>

#include <cinttypes>
#include <memory>

using namespace Swim3;

Swim3Ctrl::Swim3Ctrl()
{
    this->setup_reg = 0;
    this->mode_reg  = 0;
    this->int_reg   = 0;
    this->int_mask  = 0;
    this->xfer_cnt  = 0;
    this->first_sec = 0xFF;

    // Attach virtual Superdrive to the internal drive connector
    // TODO: make SWIM3/drive wiring user selectable
    this->int_drive = std::unique_ptr<MacSuperdrive::MacSuperDrive>
        (new MacSuperdrive::MacSuperDrive());
}

uint8_t Swim3Ctrl::read(uint8_t reg_offset)
{
    uint8_t status_addr;

    switch(reg_offset) {
    case Swim3Reg::Phase:
        return this->phase_lines;
    case Swim3Reg::Setup:
        return this->setup_reg;
    case Swim3Reg::Handshake_Mode1:
        if (this->mode_reg & 2) { // internal drive?
            status_addr = ((this->mode_reg & 0x20) >> 2) | (this->phase_lines & 7);
            return ((this->int_drive->status(status_addr) & 1) << 2);
        } else {
            return 4;
        }
    default:
        LOG_F(INFO, "SWIM3: reading from 0x%X register", reg_offset);
    }
    return 0;
}

void Swim3Ctrl::write(uint8_t reg_offset, uint8_t value)
{
    switch(reg_offset) {
    case Swim3Reg::Param_Data:
        this->pram = value;
        break;
    case Swim3Reg::Phase:
        this->phase_lines = value & 0xF;
        if (value & 8) {
            if (this->mode_reg & 2) { // internal drive?
                this->int_drive->command(
                    ((this->mode_reg & 0x20) >> 3) | (this->phase_lines & 3),
                    (value >> 2) & 1
                );
            }
        }
        break;
    case Swim3Reg::Setup:
        this->setup_reg = value;
        break;
    case Swim3Reg::Status_Mode0:
        // ones in value clear the corresponding bits in the mode register
        this->mode_reg &= ~value;
        break;
    case Swim3Reg::Handshake_Mode1:
        // ones in value set the corresponding bits in the mode register
        this->mode_reg |= value;
        break;
    case Swim3Reg::Interrupt_Mask:
        this->int_mask = value;
        break;
    default:
        LOG_F(INFO, "SWIM3: writing 0x%X to register 0x%X", value, reg_offset);
    }
}
