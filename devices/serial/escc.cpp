/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#include <core/timermanager.h>
#include <devices/deviceregistry.h>
#include <devices/serial/chario.h>
#include <devices/serial/escc.h>
#include <devices/serial/z85c30.h>
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

    this->master_int_cntrl = 0;
    this->reset();
}

void EsccController::reset()
{
    this->master_int_cntrl &= (WR9_NO_VECTOR | WR9_VECTOR_INCLUDES_STATUS);
    this->master_int_cntrl |= WR9_FORCE_HARDWARE_RESET;
    this->reg_ptr = WR0; // or RR0

    this->ch_a->reset(true);
    this->ch_b->reset(true);
}

uint8_t EsccController::read(uint8_t reg_offset)
{
    uint8_t value;

    switch(reg_offset) {
    case EsccReg::Port_B_Cmd:
        value = this->read_internal(this->ch_b.get());
        break;
    case EsccReg::Port_A_Cmd:
        value = this->read_internal(this->ch_a.get());
        break;
    case EsccReg::Port_B_Data:
        value = this->ch_b->receive_byte();
        break;
    case EsccReg::Port_A_Data:
        value = this->ch_a->receive_byte();
        break;
    case EsccReg::Enh_Reg_B:
        value = this->ch_b->get_enh_reg();
        break;
    case EsccReg::Enh_Reg_A:
        value = this->ch_a->get_enh_reg();
        break;
    default:
        LOG_F(WARNING, "ESCC: reading from unimplemented register 0x%x", reg_offset);
        value = 0;
    }

    return value;
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
    case EsccReg::Enh_Reg_B:
        this->ch_b->set_enh_reg(value);
        break;
    case EsccReg::Enh_Reg_A:
        this->ch_a->set_enh_reg(value);
        break;
    default:
        LOG_F(9, "ESCC: writing 0x%X to unimplemented register 0x%x", value, reg_offset);
    }
}

uint8_t EsccController::read_internal(EsccChannel *ch)
{
    uint8_t value;
    switch (this->reg_ptr) {
    case RR2:
        // TODO: implement interrupt vector modifications
        value = this->int_vec;
        break;
    default:
        value = ch->read_reg(this->reg_ptr);
    }
    this->reg_ptr = RR0; // or WR0
    return value;
}

void EsccController::write_internal(EsccChannel *ch, uint8_t value)
{
    switch (this->reg_ptr) {
    // chip-specific registers
    case WR0:
        this->reg_ptr = value & WR0_REGISTER_SELECTION_CODE;
        switch (value & WR0_COMMAND_CODES) {
        case WR0_COMMAND_POINT_HIGH:
            this->reg_ptr |= WR8; // or RR8
            break;
        }
        return;
    case WR2:
        this->int_vec = value;
        break;
    case WR9:
        // see if some reset is requested
        switch (value & WR9_RESET_COMMAND_BITS) {
        case WR9_CHANNEL_RESET_B:
            this->master_int_cntrl &= ~WR9_INTERRUPT_MASKING_WITHOUT_INTACK;
            this->ch_b->reset(false);
            break;
        case WR9_CHANNEL_RESET_A:
            this->master_int_cntrl &= ~WR9_INTERRUPT_MASKING_WITHOUT_INTACK;
            this->ch_a->reset(false);
            break;
        case WR9_FORCE_HARDWARE_RESET:
            this->reset();
            break;
        }

        this->master_int_cntrl = value & WR9_INTERRUPT_CONTROL_BITS;
        break;
    default:
        // channel-specific registers
        ch->write_reg(this->reg_ptr, value);
    }
    this->reg_ptr = WR0; // or RR0
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

    /*
        We use hex values here instead of enums to more
        easily compare with the z85c30 data sheet.
    */

    this->write_regs[WR0] = 0;
    this->write_regs[WR1] &= 0x24;
    this->write_regs[WR3] &= 0xFE;
    this->write_regs[WR4] |= 0x04;
    this->write_regs[WR5] &= 0x61;
    this->write_regs[WR15] = 0xF8;

    this->read_regs[RR0] &= 0x38;
    this->read_regs[RR0] |= 0x44;
    this->read_regs[RR1]  = 0x06 | RR1_ALL_SENT; // HACK: also set ALL_SENT flag.
    this->read_regs[RR3]  = 0x00;
    this->read_regs[RR10] = 0x00;

    // initialize DPLL
    this->dpll_active    = 0;
    this->dpll_mode      = DpllMode::NRZI;
    this->dpll_clock_src = 0;

    // initialize Baud Rate Generator (BRG)
    this->brg_active    = 0;
    this->brg_clock_src = 0;

    if (hw_reset) {
        this->write_regs[WR9] &= 0x03; // clear all except (WR9_NO_VECTOR | WR9_VECTOR_INCLUDES_STATUS)
        this->write_regs[WR9] |= 0xC0; // set WR9_FORCE_HARDWARE_RESET
        this->write_regs[WR10] = 0;
        this->write_regs[WR11] = 8;
        this->write_regs[WR14] &= 0xC0;
    } else {
        this->write_regs[WR9] &= ~0x20; // clear WR9_INTERRUPT_MASKING_WITHOUT_INTACK
        this->write_regs[WR10] &= 0x60;
        this->write_regs[WR14] &= 0xC3;
    }
    this->write_regs[WR14] |= 0x20;
}

void EsccChannel::write_reg(int reg_num, uint8_t value)
{
    switch (reg_num) {
    case WR3:
        if ((this->write_regs[WR3] ^ value) & WR3_ENTER_HUNT_MODE) {
            this->write_regs[WR3] |= WR3_ENTER_HUNT_MODE;
            this->read_regs[RR0] |= RR0_SYNC_HUNT;
            LOG_F(9, "%s: Hunt mode entered.", this->name.c_str());
        }
        if ((this->write_regs[WR3] ^ value) & WR3_RX_ENABLE) {
            if (value & WR3_RX_ENABLE) {
                this->write_regs[WR3] |= WR3_RX_ENABLE;
                this->chario->rcv_enable();
                LOG_F(9, "%s: receiver enabled.", this->name.c_str());
            } else {
                this->write_regs[WR3] ^= WR3_RX_ENABLE;
                this->chario->rcv_disable();
                LOG_F(9, "%s: receiver disabled.", this->name.c_str());
                this->write_regs[WR3] |= WR3_ENTER_HUNT_MODE;
                this->read_regs[RR0] |= RR0_SYNC_HUNT;
            }
        }
        this->write_regs[WR3] =
            (this->write_regs[WR3] & (WR3_RX_ENABLE | WR3_ENTER_HUNT_MODE)) |
            (value & ~(WR3_RX_ENABLE | WR3_ENTER_HUNT_MODE));
        return;
    case WR7:
        if (this->write_regs[WR15] & WR15_SDLC_HDLC_ENHANCEMENT_ENABLE) {
            this->wr7_enh = value;
            return;
        }
        break;
    case WR8:
        this->send_byte(value);
        return;
    case WR14:
        switch (value & WR14_DPLL_COMMAND_BITS) {
        case WR14_DPLL_NULL_COMMAND:
            break;
        case WR14_DPLL_ENTER_SEARCH_MODE:
            this->dpll_active = 1;
            this->read_regs[RR10] &= ~(RR10_TWO_CLOCKS_MISSING | RR10_ONE_CLOCK_MISSING);
            break;
        case WR14_DPLL_RESET_MISSING_CLOCK:
            this->read_regs[RR10] &= ~(RR10_TWO_CLOCKS_MISSING | RR10_ONE_CLOCK_MISSING);
            break;
        case WR14_DPLL_DISABLE_DPLL:
            this->dpll_active = 0;
            // fallthrough
        case WR14_DPLL_SET_SOURCE_BR_GENERATOR:
            this->dpll_clock_src = 0;
            break;
        case WR14_DPLL_SET_SOURCE_RTXC:
            this->dpll_clock_src = 1;
            break;
        case WR14_DPLL_SET_FM_MODE:
            this->dpll_mode = DpllMode::FM;
            break;
        case WR14_DPLL_SET_NRZI_MODE:
            this->dpll_mode = DpllMode::NRZI;
            break;
        }
        if (value & (WR14_LOCAL_LOOPBACK | WR14_AUTO_ECHO | WR14_DTR_REQUEST_FUNCTION)) {
            LOG_F(WARNING, "%s: unexpected value in WR14 = 0x%X", this->name.c_str(), value);
        }
        if (this->brg_active ^ (value & WR14_BR_GENERATOR_ENABLE)) {
            this->brg_active = value & WR14_BR_GENERATOR_ENABLE;
            LOG_F(9, "%s: BRG %s", this->name.c_str(), this->brg_active ? "enabled" : "disabled");
        }
        return;
    }

    this->write_regs[reg_num] = value;
}

uint8_t EsccChannel::read_reg(int reg_num)
{
    switch (reg_num) {
    case RR0:
        if (this->chario->rcv_char_available()) {
            return this->read_regs[RR0] |= RR0_RX_CHARACTER_AVAILABLE;
        } else {
            return this->read_regs[RR0] &= ~RR0_RX_CHARACTER_AVAILABLE;
        }
        break;
    case RR8:
        return this->receive_byte();
    }
    return this->read_regs[reg_num];
}

void EsccChannel::send_byte(uint8_t value)
{
    // TODO: put one byte into the Data FIFO

    this->write_regs[WR8] = value;
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
    this->read_regs[RR0] &= ~RR0_RX_CHARACTER_AVAILABLE;
    this->read_regs[RR8] = c;
    return c;
}

uint8_t EsccChannel::get_enh_reg()
{
    return this->enh_reg;
}

void EsccChannel::set_enh_reg(uint8_t value)
{
    uint8_t changed_bits = value ^ this->enh_reg;
    if (changed_bits & 0x10) {
        if (value & 0x10)
            LOG_F(ERROR, "%s: CTS connected to GPIO; DCD connected to GND", this->name.c_str());
        else
            LOG_F(INFO, "%s: CTS connected to TRXC_In_l; DCD connected to GPIO", this->name.c_str());
        this->enh_reg = value & 0x10;
    } else if (changed_bits & ~0x10) {
        if (value & ~0x10)
            LOG_F(ERROR, "%s: Ignoring attempt to set Enh_Reg bits 0x%02x", this->name.c_str(), value & ~0x10);
    }
}

void EsccChannel::dma_start_tx()
{

}

void EsccChannel::dma_start_rx()
{

}

void EsccChannel::dma_stop_tx()
{
    if (this->timer_id_tx) {
        TimerManager::get_instance()->cancel_timer(this->timer_id_tx);
        this->timer_id_tx = 0;
    }
}

void EsccChannel::dma_stop_rx()
{
    if (this->timer_id_rx) {
        TimerManager::get_instance()->cancel_timer(this->timer_id_rx);
        this->timer_id_rx = 0;
    }
}

void EsccChannel::dma_in_tx()
{
    LOG_F(ERROR, "%s: Unexpected DMA INPUT command for transmit.", this->name.c_str());
}

void EsccChannel::dma_in_rx()
{
    if (dma_ch[1]->get_push_data_remaining()) {
        this->timer_id_rx = TimerManager::get_instance()->add_oneshot_timer(
            0,
            [this]() {
                this->timer_id_rx = 0;
                char c = receive_byte();
                dma_ch[1]->push_data(&c, 1);
                this->dma_in_rx();
        });
    }
}

void EsccChannel::dma_out_tx()
{
    this->timer_id_tx = TimerManager::get_instance()->add_oneshot_timer(
        10,
        [this]() {
            this->timer_id_tx = 0;
            uint8_t *data;
            uint32_t avail_len;

            if (dma_ch[1]->pull_data(256, &avail_len, &data) == MoreData) {
                while(avail_len) {
                    this->send_byte(*data++);
                    avail_len--;
                }
                this->dma_out_tx();
            }
    });
}

void EsccChannel::dma_out_rx()
{
    LOG_F(ERROR, "%s: Unexpected DMA OUTPUT command for receive.", this->name.c_str());
}

void EsccChannel::dma_flush_tx()
{
    this->dma_stop_tx();
    this->timer_id_tx = TimerManager::get_instance()->add_oneshot_timer(
        10,
        [this]() {
            this->timer_id_tx = 0;
            dma_ch[1]->end_pull_data();
    });
}

void EsccChannel::dma_flush_rx()
{
    this->dma_stop_rx();
    this->timer_id_rx = TimerManager::get_instance()->add_oneshot_timer(
        10,
        [this]() {
            this->timer_id_rx = 0;
            dma_ch[1]->end_push_data();
    });
}

static const std::vector<std::string> CharIoBackends = {"null", "stdio", "socket"};

static const PropMap Escc_Properties = {
    {"serial_backend", new StrProperty("null", CharIoBackends)},
};

static const DeviceDescription Escc_Descriptor = {
    EsccController::create, {}, Escc_Properties, HWCompType::MMIO_DEV
};

REGISTER_DEVICE(Escc, Escc_Descriptor);
