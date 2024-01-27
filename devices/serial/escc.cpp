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

/** @file Enhanced Serial Communications Controller (ESCC) emulation. */

#include <devices/deviceregistry.h>
#include <devices/serial/chario.h>
#include <devices/serial/escc.h>
#include <loguru.hpp>
#include <machines/machineproperties.h>

#include <cinttypes>
#include <memory>
#include <string>
#include <vector>

/** Remap the compatible addressing scheme to MacRISC one. */
const uint8_t compat_to_macrisc[6] = {
    EsccReg::Port_B_Cmd,    EsccReg::Port_A_Cmd,
    EsccReg::Port_B_Data,   EsccReg::Port_A_Data,
    EsccReg::Enh_Reg_B,     EsccReg::Enh_Reg_A
};

EsccController::EsccController()
{
    // allocate channels
    this->ch_a = std::unique_ptr<EsccChannel> (new EsccChannel("ESCC_A"));
    this->ch_b = std::unique_ptr<EsccChannel> (new EsccChannel("ESCC_B"));

    // attach backends
    std::string backend_name = GET_STR_PROP("serial_backend");

    this->ch_a->attach_backend(
        (backend_name == "stdio") ? CHARIO_BE_STDIO :
#ifdef _WIN32
#else
        (backend_name == "socket") ? CHARIO_BE_SOCKET :
#endif
        CHARIO_BE_NULL
    );
    this->ch_b->attach_backend(CHARIO_BE_NULL);

    this->reg_ptr = 0;
}

void EsccController::reset()
{
    this->master_int_cntrl &= 0xFC;
    this->master_int_cntrl |= 0xC0;

    this->ch_a->reset(true);
    this->ch_b->reset(true);
}

uint8_t EsccController::read(uint8_t reg_offset)
{
    uint8_t result = 0;

    switch(reg_offset) {
    case EsccReg::Port_B_Cmd:
        LOG_F(9, "ESCC: reading Port B register RR%d", this->reg_ptr);
        if (this->reg_ptr == 2) {
            // TODO: implement interrupt vector modifications
            result = this->int_vec;
        } else {
            result = this->ch_b->read_reg(this->reg_ptr);
        }
        this->reg_ptr = 0;
        break;
    case EsccReg::Port_A_Cmd:
        LOG_F(9, "ESCC: reading Port A register RR%d", this->reg_ptr);
        if (this->reg_ptr == 2) {
            return this->int_vec;
        } else {
            return this->ch_a->read_reg(this->reg_ptr);
        }
        break;
    case EsccReg::Port_B_Data:
        return this->ch_b->receive_byte();
    case EsccReg::Port_A_Data:
        return this->ch_a->receive_byte();
    default:
        LOG_F(9, "ESCC: reading from unimplemented register 0x%x", reg_offset);
    }

    return result;
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
    case EsccReg::Port_B_Data:
        this->ch_b->send_byte(value);
        break;
    case EsccReg::Port_A_Data:
        this->ch_a->send_byte(value);
        break;
    default:
        LOG_F(9, "ESCC: writing 0x%X to unimplemented register 0x%x", value, reg_offset);
    }
}

void EsccController::write_internal(EsccChannel *ch, uint8_t value)
{
    if (this->reg_ptr) {
        // chip-specific registers
        if (this->reg_ptr == 9) {
            // see if some reset is requested
            switch(value & 0xC0) {
            case RESET_CH_B:
                this->master_int_cntrl &= 0xDF;
                this->ch_b->reset(false);
                break;
            case RESET_CH_A:
                this->master_int_cntrl &= 0xDF;
                this->ch_a->reset(false);
                break;
            case RESET_ESCC:
                this->reset();
                break;
            }

            this->master_int_cntrl = value & 0x3F;
        } else if (this->reg_ptr == 2) {
            this->int_vec = value;
        } else { // channel-specific registers
            ch->write_reg(this->reg_ptr, value);
        }
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

// ======================== ESCC Channel methods ==============================
void EsccChannel::attach_backend(int id)
{
    switch(id) {
    case CHARIO_BE_NULL:
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoNull);
        break;
    case CHARIO_BE_STDIO:
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoStdin);
        break;
#ifdef _WIN32
#else
    case CHARIO_BE_SOCKET:
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoSocket);
        break;
#endif
    default:
        LOG_F(ERROR, "%s: unknown backend ID %d, using NULL instead", this->name.c_str(), id);
        this->chario = std::unique_ptr<CharIoBackEnd> (new CharIoNull);
    }
}

void EsccChannel::reset(bool hw_reset)
{
    this->chario->rcv_disable();

    this->write_regs[1] &= 0x24;
    this->write_regs[3] &= 0xFE;
    this->write_regs[4] |= 0x04;
    this->write_regs[5] &= 0x61;
    this->write_regs[15] = 0xF8;

    this->read_regs[0] &= 0x3C;
    this->read_regs[0] |= 0x44;
    this->read_regs[1]  = 0x06;
    this->read_regs[3]  = 0x00;
    this->read_regs[10] = 0x00;

    // initialize DPLL
    this->dpll_active    = 0;
    this->dpll_mode      = DpllMode::NRZI;
    this->dpll_clock_src = 0;

    // initialize Baud Rate Generator (BRG)
    this->brg_active    = 0;
    this->brg_clock_src = 0;

    if (hw_reset) {
        this->write_regs[10] = 0;
        this->write_regs[11] = 8;
        this->write_regs[14] &= 0xC0;
        this->write_regs[14] |= 0x20;
    } else {
        this->write_regs[10] &= 0x60;
        this->write_regs[14] &= 0xC3;
        this->write_regs[14] |= 0x20;
    }
}

void EsccChannel::write_reg(int reg_num, uint8_t value)
{
    switch (reg_num) {
    case 3:
        if ((this->write_regs[3] ^ value) & 0x10) {
            this->write_regs[3] |= 0x10;
            this->read_regs[0] |= 0x10; // set SYNC_HUNT flag
            LOG_F(9, "%s: Hunt mode entered.", this->name.c_str());
        }
        if ((this->write_regs[3] ^ value) & 1) {
            if (value & 1) {
                this->write_regs[3] |= 0x1;
                this->chario->rcv_enable();
                LOG_F(9, "%s: receiver enabled.", this->name.c_str());
            } else {
                this->write_regs[3] ^= 0x1;
                this->chario->rcv_disable();
                LOG_F(9, "%s: receiver disabled.", this->name.c_str());
                this->write_regs[3] |= 0x10; // enter HUNT mode
                this->read_regs[0] |= 0x10; // set SYNC_HUNT flag
            }
        }
        this->write_regs[3] = (this->write_regs[3] & 0x11) | (value & 0xEE);
        return;
    case 7:
        if (this->write_regs[15] & 1) {
            this->wr7_enh = value;
            return;
        }
        break;
    case 14:
        switch (value >> 5) {
        case DPLL_NULL_CMD:
            break;
        case DPLL_ENTER_SRC_MODE:
            this->dpll_active = 1;
            this->read_regs[10] &= 0x3F;
            break;
        case DPLL_DISABLE:
            this->dpll_active = 0;
            // fallthrough
        case DPLL_RST_MISSING_CLK:
            this->read_regs[10] &= 0x3F;
            break;
        case DPLL_SET_SRC_BGR:
            this->dpll_clock_src = 0;
            break;
        case DPLL_SET_SRC_RTXC:
            this->dpll_clock_src = 1;
            break;
        case DPLL_SET_FM_MODE:
            this->dpll_mode = DpllMode::FM;
            break;
        case DPLL_SET_NRZI_MODE:
            this->dpll_mode = DpllMode::NRZI;
            break;
        default:
            LOG_F(WARNING, "%s: unimplemented DPLL command %d", this->name.c_str(), value >> 5);
        }
        if (value & 0x1C) { // Local Loopback, Auto Echo DTR/REQ bits set
            LOG_F(WARNING, "%s: unexpected value in WR14 = 0x%X", this->name.c_str(), value);
        }
        if (this->brg_active ^ (value & 1)) {
            this->brg_active = value & 1;
            LOG_F(9, "%s: BRG %s", this->name.c_str(), this->brg_active ? "enabled" : "disabled");
        }
        return;
    }

    this->write_regs[reg_num] = value;
}

uint8_t EsccChannel::read_reg(int reg_num)
{
    if (!reg_num) {
        if (this->chario->rcv_char_available()) {
            this->read_regs[0] |= 1;
        }
    }
    return this->read_regs[reg_num];
}

void EsccChannel::send_byte(uint8_t value)
{
    // TODO: put one byte into the Data FIFO

    this->chario->xmit_char(value);
}

uint8_t EsccChannel::receive_byte()
{
    // TODO: remove one byte from the Receive FIFO

    uint8_t c;

    if (this->chario->rcv_char_available_now()) {
        this->chario->rcv_char(&c);
    } else {
        c = 0;
    }
    this->read_regs[0] &= ~1;
    return c;
}

static const vector<string> CharIoBackends = {"null", "stdio", "socket"};

static const PropMap Escc_Properties = {
    {"serial_backend", new StrProperty("null", CharIoBackends)},
};

static const DeviceDescription Escc_Descriptor = {
    EsccController::create, {}, Escc_Properties
};

REGISTER_DEVICE(Escc, Escc_Descriptor);
