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

/** @file NCR53C94/Am53CF94 SCSI controller emulation. */

#include "sc53c94.h"
#include <loguru.hpp>

#include <cinttypes>

void Sc53C94::reset_device()
{
    // part-unique ID to be read using a magic sequence
    this->set_xfer_count = this->chip_id << 16;

    this->clk_factor  = 2;
    this->sel_timeout = 0;
}

uint8_t Sc53C94::read(uint8_t reg_offset)
{
    switch (reg_offset) {
    case Read::Reg53C94::Xfer_Cnt_Hi:
        if (this->config2 & CFG2_ENF) {
            return (this->xfer_count >> 16) & 0xFFU;
        }
        break;
    default:
        LOG_F(9, "NCR53C94: reading from register %d", reg_offset);
    }
    return 0;
}

void Sc53C94::write(uint8_t reg_offset, uint8_t value)
{
    switch (reg_offset) {
    case Write::Reg53C94::Command:
        add_command(value);
        break;
    case Write::Reg53C94::Sel_Timeout:
        this->sel_timeout = value;
        break;
    case Write::Reg53C94::Clock_Factor:
        this->clk_factor = value;
        break;
    case Write::Reg53C94::Config_1:
        this->config1 = value;
        break;
    case Write::Reg53C94::Config_2:
        this->config2 = value;
        break;
    case Write::Reg53C94::Config_3:
        this->config3 = value;
        break;
    default:
        LOG_F(INFO, "SC53C94: writing 0x%X to %d register", value, reg_offset);
    }
}

void Sc53C94::add_command(uint8_t cmd)
{
    bool is_dma_cmd = !!(cmd & 0x80);

    cmd &= 0x7F;

    if (this->on_reset && cmd != CMD_NOP) {
        LOG_F(WARNING, "SC53C94: command register blocked after RESET!");
        return;
    }

    // NOTE: Reset Device (chip), Reset Bus and DMA Stop commands execute
    // immediately while all others are placed into the command FIFO
    switch (cmd) {
    case CMD_RESET_DEVICE:
        reset_device();
        this->on_reset = true; // block the command register
        return;
    case CMD_RESET_BUS:
        LOG_F(ERROR, "SC53C94: RESET_BUS command not implemented yet");
        return;
    case CMD_DMA_STOP:
        LOG_F(ERROR, "SC53C94: TARGET_DMA_STOP command not implemented yet");
        return;
    }

    // HACK: commands should be placed into the command FIFO
    if (cmd == CMD_NOP) {
        if (is_dma_cmd) {
            if (this->config2 & CFG2_ENF) { // extended mode: 24-bit
                this->xfer_count = this->set_xfer_count & 0xFFFFFFUL;
            } else { // standard mode: 16-bit
                this->xfer_count = this->set_xfer_count & 0xFFFFUL;
            }
        }
        this->on_reset = false;
    }
}
