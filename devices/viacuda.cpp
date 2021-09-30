/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

/** VIA-CUDA combo device emulation.

    Author: Max Poliakovski 2019
*/

#include "viacuda.h"
#include "adb.h"
#include <cinttypes>
#include <loguru.hpp>

using namespace std;

ViaCuda::ViaCuda() {
    this->name = "ViaCuda";

    /* FIXME: is this the correct
       VIA initialization? */
    this->via_regs[VIA_A]    = 0x80;
    this->via_regs[VIA_DIRB] = 0xFF;
    this->via_regs[VIA_DIRA] = 0xFF;
    this->via_regs[VIA_T1LL] = 0xFF;
    this->via_regs[VIA_T1LH] = 0xFF;
    this->via_regs[VIA_IER]  = 0x7F;

    // PRAM Pre-Initialization
    this->pram_obj = new NVram("pram.bin", 256);

    this->adb_obj = new ADB_Bus();

    this->init();
}

ViaCuda::~ViaCuda() {
    if (this->pram_obj)
        delete (this->pram_obj);
}

void ViaCuda::init() {
    this->old_tip     = 0;
    this->old_byteack = 0;
    this->treq        = 1;
    this->in_count    = 0;
    this->out_count   = 0;
    this->poll_rate   = 11;
}

uint8_t ViaCuda::read(int reg) {
    uint8_t res;

    LOG_F(9, "Read VIA reg %x \n", (uint32_t)reg);

    res = this->via_regs[reg & 0xF];

    /* reading from some VIA registers triggers special actions */
    switch (reg & 0xF) {
    case VIA_B:
        res = this->via_regs[VIA_B];
        break;
    case VIA_A:
    case VIA_ANH:
        LOG_F(WARNING, "Attempted read from VIA Port A! \n");
        break;
    case VIA_IER:
        res |= 0x80; /* bit 7 always reads as "1" */
    }

    return res;
}

void ViaCuda::write(int reg, uint8_t value) {
    switch (reg & 0xF) {
    case VIA_B:
        this->via_regs[VIA_B] = value;
        write(value);
        break;
    case VIA_A:
    case VIA_ANH:
        LOG_F(WARNING, "Attempted write to VIA Port A! \n");
        break;
    case VIA_DIRB:
        LOG_F(9, "VIA_DIRB = %x \n", (uint32_t)value);
        this->via_regs[VIA_DIRB] = value;
        break;
    case VIA_DIRA:
        LOG_F(9, "VIA_DIRA = %x \n", (uint32_t)value);
        this->via_regs[VIA_DIRA] = value;
        break;
    case VIA_PCR:
        LOG_F(9, "VIA_PCR =  %x \n", (uint32_t)value);
        this->via_regs[VIA_PCR] = value;
        break;
    case VIA_ACR:
        LOG_F(9, "VIA_ACR =  %x \n", (uint32_t)value);
        this->via_regs[VIA_ACR] = value;
        break;
    case VIA_IER:
        this->via_regs[VIA_IER] = (value & 0x80) ? value & 0x7F : this->via_regs[VIA_IER] & ~value;
        LOG_F(INFO, "VIA_IER updated to %d \n", (uint32_t)this->via_regs[VIA_IER]);
        print_enabled_ints();
        break;
    default:
        this->via_regs[reg & 0xF] = value;
    }
}

void ViaCuda::print_enabled_ints() {
    const char* via_int_src[] = {"CA2", "CA1", "SR", "CB2", "CB1", "T2", "T1"};

    for (int i = 0; i < 7; i++) {
        if (this->via_regs[VIA_IER] & (1 << i))
            LOG_F(INFO, "VIA %s interrupt enabled \n", via_int_src[i]);
    }
}

inline bool ViaCuda::ready() {
    return ((this->via_regs[VIA_DIRB] & 0x38) == 0x30);
}

inline void ViaCuda::assert_sr_int() {
    this->via_regs[VIA_IFR] |= 0x84;
}

void ViaCuda::write(uint8_t new_state) {
    if (!ready()) {
        LOG_F(WARNING, "Cuda not ready! \n");
        return;
    }

    int new_tip     = !!(new_state & CUDA_TIP);
    int new_byteack = !!(new_state & CUDA_BYTEACK);

    /* return if there is no state change */
    if (new_tip == this->old_tip && new_byteack == this->old_byteack)
        return;

    LOG_F(9, "Cuda state changed! \n");

    this->old_tip     = new_tip;
    this->old_byteack = new_byteack;

    if (new_tip) {
        if (new_byteack) {
            this->via_regs[VIA_B] |= CUDA_TREQ; /* negate TREQ */
            this->treq = 1;

            if (this->in_count) {
                process_packet();

                /* start response transaction */
                this->via_regs[VIA_B] &= ~CUDA_TREQ; /* assert TREQ */
                this->treq = 0;
            }

            this->in_count = 0;
        } else {
            LOG_F(9, "Cuda: enter sync state \n");
            this->via_regs[VIA_B] &= ~CUDA_TREQ; /* assert TREQ */
            this->treq      = 0;
            this->in_count  = 0;
            this->out_count = 0;
        }

        assert_sr_int(); /* send dummy byte as idle acknowledge or attention */
    } else {
        if (this->via_regs[VIA_ACR] & 0x10) { /* data transfer: Host --> Cuda */
            if (this->in_count < 16) {
                this->in_buf[this->in_count++] = this->via_regs[VIA_SR];
                assert_sr_int(); /* tell the system we've read the data */
            } else {
                LOG_F(WARNING, "Cuda input buffer exhausted! \n");
            }
        } else { /* data transfer: Cuda --> Host */
            (this->*out_handler)();
            assert_sr_int(); /* tell the system we've written the data */
        }
    }
}

/* sends zeros to host at infinitum */
void ViaCuda::null_out_handler() {
    this->via_regs[VIA_SR] = 0;
}

/* sends data from out_buf until exhausted, then switches to next_out_handler */
void ViaCuda::out_buf_handler() {
    if (this->out_pos < this->out_count) {
        LOG_F(9, "OutBufHandler: sending next byte 0x%X", this->out_buf[this->out_pos]);
        this->via_regs[VIA_SR] = this->out_buf[this->out_pos++];
    } else if (this->is_open_ended) {
        LOG_F(9, "OutBufHandler: switching to next handler");
        this->out_handler      = this->next_out_handler;
        this->next_out_handler = &ViaCuda::null_out_handler;
        (this->*out_handler)();
    } else {
        LOG_F(9, "Sending last byte");
        this->out_count = 0;
        this->via_regs[VIA_B] |= CUDA_TREQ; /* negate TREQ */
        this->treq = 1;
    }
}

void ViaCuda::response_header(uint32_t pkt_type, uint32_t pkt_flag) {
    this->out_buf[0]       = pkt_type;
    this->out_buf[1]       = pkt_flag;
    this->out_buf[2]       = this->in_buf[1]; /* copy original cmd */
    this->out_count        = 3;
    this->out_pos          = 0;
    this->out_handler      = &ViaCuda::out_buf_handler;
    this->next_out_handler = &ViaCuda::null_out_handler;
    this->is_open_ended    = false;
}

void ViaCuda::error_response(uint32_t error) {
    this->out_buf[0]       = CUDA_PKT_ERROR;
    this->out_buf[1]       = error;
    this->out_buf[2]       = this->in_buf[0];
    this->out_buf[3]       = this->in_buf[1]; /* copy original cmd */
    this->out_count        = 4;
    this->out_pos          = 0;
    this->out_handler      = &ViaCuda::out_buf_handler;
    this->next_out_handler = &ViaCuda::null_out_handler;
    this->is_open_ended    = false;
}

void ViaCuda::process_packet() {
    if (this->in_count < 2) {
        LOG_F(ERROR, "Cuda: invalid packet (too few data)!\n");
        error_response(CUDA_ERR_BAD_SIZE);
        return;
    }

    switch (this->in_buf[0]) {
    case CUDA_PKT_ADB:
        LOG_F(9, "Cuda: ADB packet received \n");
        process_adb_command(this->in_buf[1], this->in_count - 2);
        break;
    case CUDA_PKT_PSEUDO:
        LOG_F(9, "Cuda: pseudo command packet received \n");
        LOG_F(9, "Command: %x \n", (uint32_t)(this->in_buf[1]));
        LOG_F(9, "Data count: %d \n ", this->in_count);
        for (int i = 0; i < this->in_count; i++) {
            LOG_F(9, "%x ,", (uint32_t)(this->in_buf[i]));
        }
        pseudo_command(this->in_buf[1], this->in_count - 2);
        break;
    default:
        LOG_F(ERROR, "Cuda: unsupported packet type = %d \n", (uint32_t)(this->in_buf[0]));
        error_response(CUDA_ERR_BAD_PKT);
    }
}

void ViaCuda::process_adb_command(uint8_t cmd_byte, int data_count) {
    int adb_dev = cmd_byte >> 4;    // 2 for keyboard, 3 for mouse
    int cmd     = cmd_byte & 0xF;

    if (!cmd) {
        LOG_F(9, "Cuda: ADB SendReset command requested\n");
        response_header(CUDA_PKT_ADB, 0);
    } else if (cmd == 1) {
        LOG_F(9, "Cuda: ADB Flush command requested\n");
        response_header(CUDA_PKT_ADB, 0);
    } else if ((cmd & 0xC) == 8) {
        LOG_F(9, "Cuda: ADB Listen command requested\n");
        int adb_reg = cmd_byte & 0x3;
        if (adb_obj->listen(adb_dev, adb_reg)) {
            response_header(CUDA_PKT_ADB, 0);
            for (int data_ptr = 0; data_ptr < adb_obj->get_output_len(); data_ptr++) {
                this->in_buf[(2 + data_ptr)] = adb_obj->get_output_byte(data_ptr);
            }
        } else {
            response_header(CUDA_PKT_ADB, 2);
        }
    } else if ((cmd & 0xC) == 0xC) {
        LOG_F(9, "Cuda: ADB Talk command requested\n");
        response_header(CUDA_PKT_ADB, 0);
        int adb_reg = cmd_byte & 0x3;
        if (adb_obj->talk(adb_dev, adb_reg, this->in_buf[2])) {
            response_header(CUDA_PKT_ADB, 0);
        } else {
            response_header(CUDA_PKT_ADB, 2);
        }
    } else {
        LOG_F(ERROR, "Cuda: unsupported ADB command 0x%x \n", cmd);
        error_response(CUDA_ERR_BAD_CMD);
    }
}

void ViaCuda::pseudo_command(int cmd, int data_count) {
    switch (cmd) {
    case CUDA_START_STOP_AUTOPOLL:
        if (this->in_buf[2]) {
            LOG_F(INFO, "Cuda: autopoll started, rate: %dms", this->poll_rate);
        } else {
            LOG_F(INFO, "Cuda: autopoll stopped");
        }
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    case CUDA_READ_PRAM:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->pram_obj->read_byte(this->in_buf[2]);
        break;
    case CUDA_WRITE_PRAM:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->pram_obj->write_byte(this->in_buf[2], this->in_buf[3]);
        break;
    case CUDA_SET_AUTOPOLL_RATE:
        this->poll_rate = this->in_buf[2];
        LOG_F(INFO, "Cuda: autopoll rate set to: %d", this->poll_rate);
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    case CUDA_GET_AUTOPOLL_RATE:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->out_buf[3] = this->poll_rate;
        this->out_count++;
        break;
    case CUDA_READ_WRITE_I2C:
        response_header(CUDA_PKT_PSEUDO, 0);
        i2c_simple_transaction(this->in_buf[2], &this->in_buf[3], this->in_count - 3);
        break;
    case CUDA_COMB_FMT_I2C:
        response_header(CUDA_PKT_PSEUDO, 0);
        if (this->in_count >= 5) {
            i2c_comb_transaction(
                this->in_buf[2], this->in_buf[3], this->in_buf[4], &this->in_buf[5], this->in_count - 5);
        }
        break;
    case CUDA_OUT_PB0: /* undocumented call! */
        LOG_F(INFO, "Cuda: send %d to PB0 \n", (int)(this->in_buf[2]));
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    default:
        LOG_F(ERROR, "Cuda: unsupported pseudo command 0x%x \n", cmd);
        error_response(CUDA_ERR_BAD_CMD);
    }
}

/* sends data from the current I2C to host ad infinitum */
void ViaCuda::i2c_handler() {
    this->receive_byte(this->curr_i2c_addr, &this->via_regs[VIA_SR]);
}

void ViaCuda::i2c_simple_transaction(uint8_t dev_addr, const uint8_t* in_buf, int in_bytes) {
    int op_type = dev_addr & 1; /* 0 - write to device, 1 - read from device */

    dev_addr >>= 1; /* strip RD/WR bit */

    if (!this->start_transaction(dev_addr)) {
        LOG_F(WARNING, "Unsupported I2C device 0x%X \n", (int)(dev_addr));
        error_response(CUDA_ERR_I2C);
        return;
    }

    /* send data to the target I2C device until there is no more data to send
       or the target device doesn't acknowledge that indicates an error */
    for (int i = 0; i < in_bytes; i++) {
        if (!this->send_byte(dev_addr, in_buf[i])) {
            LOG_F(WARNING, "NO_ACK during sending, device 0x%X \n", (int)(dev_addr));
            error_response(CUDA_ERR_I2C);
            return;
        }
    }

    if (op_type) { /* read request initiate an open ended transaction */
        this->curr_i2c_addr    = dev_addr;
        this->out_handler      = &ViaCuda::out_buf_handler;
        this->next_out_handler = &ViaCuda::i2c_handler;
        this->is_open_ended    = true;
    }
}

void ViaCuda::i2c_comb_transaction(
    uint8_t dev_addr, uint8_t sub_addr, uint8_t dev_addr1, const uint8_t* in_buf, int in_bytes) {
    int op_type = dev_addr1 & 1; /* 0 - write to device, 1 - read from device */

    if ((dev_addr & 0xFE) != (dev_addr1 & 0xFE)) {
        LOG_F(ERROR, "Combined I2C: dev_addr mismatch!\n");
        error_response(CUDA_ERR_I2C);
        return;
    }

    dev_addr >>= 1; /* strip RD/WR bit */

    if (!this->start_transaction(dev_addr)) {
        LOG_F(WARNING, "Unsupported I2C device 0x%X \n", (int)(dev_addr));
        error_response(CUDA_ERR_I2C);
        return;
    }

    if (!this->send_subaddress(dev_addr, sub_addr)) {
        LOG_F(WARNING, "NO_ACK while sending subaddress, device 0x%X \n", (int)(dev_addr));
        error_response(CUDA_ERR_I2C);
        return;
    }

    /* send data to the target I2C device until there is no more data to send
       or the target device doesn't acknowledge that indicates an error */
    for (int i = 0; i < in_bytes; i++) {
        if (!this->send_byte(dev_addr, in_buf[i])) {
            LOG_F(WARNING, "NO_ACK during sending, device 0x%X \n", (int)(dev_addr));
            error_response(CUDA_ERR_I2C);
            return;
        }
    }

    if (!op_type) { /* return dummy response for writes */
        LOG_F(WARNING, "Combined I2C - write request!");
    } else {
        this->curr_i2c_addr    = dev_addr;
        this->out_handler      = &ViaCuda::out_buf_handler;
        this->next_out_handler = &ViaCuda::i2c_handler;
        this->is_open_ended    = true;
    }
}
