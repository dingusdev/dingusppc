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

Sc53C94::Sc53C94(uint8_t chip_id)
{
    this->chip_id = chip_id;
    reset_device();
}

void Sc53C94::reset_device()
{
    // part-unique ID to be read using a magic sequence
    this->set_xfer_count = this->chip_id << 16;

    this->clk_factor  = 2;
    this->sel_timeout = 0;

    // clear command FIFO
    this->cmd_fifo_pos = 0;
}

uint8_t Sc53C94::read(uint8_t reg_offset)
{
    switch (reg_offset) {
    case Read::Reg53C94::Command:
        return this->cmd_fifo[0];
    case Read::Reg53C94::Status:
        return this->status;
    case Read::Reg53C94::Int_Status:
        return this->int_status;
    case Read::Reg53C94::Xfer_Cnt_Hi:
        if (this->config2 & CFG2_ENF) {
            return (this->xfer_count >> 16) & 0xFFU;
        }
        break;
    default:
        LOG_F(INFO, "SC53C94: reading from register %d", reg_offset);
    }
    return 0;
}

void Sc53C94::write(uint8_t reg_offset, uint8_t value)
{
    switch (reg_offset) {
    case Write::Reg53C94::Command:
        update_command_reg(value);
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

void Sc53C94::update_command_reg(uint8_t cmd)
{
    if (this->on_reset && (cmd & 0x7F) != CMD_NOP) {
        LOG_F(WARNING, "SC53C94: command register blocked after RESET!");
        return;
    }

    // NOTE: Reset Device (chip), Reset Bus and DMA Stop commands execute
    // immediately while all others are placed into the command FIFO
    switch (cmd & 0x7F) {
    case CMD_RESET_DEVICE:
    case CMD_RESET_BUS:
    case CMD_DMA_STOP:
        this->cmd_fifo_pos = 0; // put them at the bottom of the command FIFO
    }

    if (this->cmd_fifo_pos < 2) {
        // put new command into the command FIFO
        this->cmd_fifo[this->cmd_fifo_pos++] = cmd;
        if (this->cmd_fifo_pos == 1) {
            exec_command();
        }
    } else {
        LOG_F(ERROR, "SC53C94: the top of the command FIFO overwritten!");
        this->status |= 0x40; // signal IOE/Gross Error
    }
}

void Sc53C94::exec_command()
{
    uint8_t cmd = this->cmd_fifo[0] & 0x7F;
    bool    is_dma_cmd = !!(this->cmd_fifo[0] & 0x80);

    if (is_dma_cmd) {
        if (this->config2 & CFG2_ENF) { // extended mode: 24-bit
            this->xfer_count = this->set_xfer_count & 0xFFFFFFUL;
        } else { // standard mode: 16-bit
            this->xfer_count = this->set_xfer_count & 0xFFFFUL;
        }
    }

    switch (cmd) {
    case CMD_NOP:
        this->on_reset = false; // unblock the command register
        exec_next_command();
        break;
    case CMD_CLEAR_FIFO:
        this->data_fifo_pos = 0; // set the bottom of the FIFO to zero
        this->data_fifo[0] = 0;
        exec_next_command();
        break;
    case CMD_RESET_DEVICE:
        reset_device();
        this->on_reset = true; // block the command register
        return;
    case CMD_RESET_BUS:
        LOG_F(INFO, "SC53C94: resetting SCSI bus...");
        exec_next_command();
        break;
    default:
        LOG_F(ERROR, "SC53C94: invalid/unimplemented command 0x%X", cmd);
        this->cmd_fifo_pos--; // remove invalid command from FIFO
        this->int_status |= 0x40; // set ICMD bit
    }
}

void Sc53C94::exec_next_command()
{
    if (this->cmd_fifo_pos) { // skip empty command FIFO
        this->cmd_fifo_pos--; // remove completed command
        if (this->cmd_fifo_pos) { // is there another command in the FIFO?
            this->cmd_fifo[0] = this->cmd_fifo[1]; // top -> bottom
            exec_command(); // execute it
        }
    }
}
