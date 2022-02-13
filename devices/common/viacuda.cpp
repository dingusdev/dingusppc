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

/** High-level VIA-CUDA combo device emulation.
 */

#include <core/timermanager.h>
#include <devices/common/adb/adb.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/viacuda.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cinttypes>
#include <memory>

using namespace std;

ViaCuda::ViaCuda() {
    this->name = "ViaCuda";

    supports_types(HWCompType::ADB_HOST | HWCompType::I2C_HOST);

    // VIA reset clears all internal registers to logic 0
    // except timers/counters and the shift register
    // as stated in the 6522 datasheet
    this->via_regs[VIA_A]    = 0;
    this->via_regs[VIA_B]    = 0;
    this->via_regs[VIA_DIRB] = 0;
    this->via_regs[VIA_DIRA] = 0;
    this->via_regs[VIA_IER]  = 0;
    this->via_regs[VIA_ACR]  = 0;
    this->via_regs[VIA_PCR]  = 0;
    this->via_regs[VIA_IFR]  = 0;

    // load maximum value into the timer registers for safety
    // (not prescribed in the 6522 datasheet)
    this->via_regs[VIA_T1LL] = 0xFF;
    this->via_regs[VIA_T1LH] = 0xFF;
    this->via_regs[VIA_T2CL] = 0xFF;
    this->via_regs[VIA_T2CH] = 0xFF;

    this->_via_ifr = 0; // all flags cleared
    this->_via_ier = 0; // all interrupts disabled
    this->irq      = 0; // IRQ is not active

    // intialize counters/timers
    this->t1_counter  = 0xFFFF;
    this->t1_active = false;
    this->t2_counter  = 0xFFFF;
    this->t2_active = false;

    // calculate VIA clock duration in ns
    this->via_clk_dur = 1.0f / VIA_CLOCK_HZ * NS_PER_SEC;

    // PRAM is part of Cuda
    this->pram_obj = std::unique_ptr<NVram> (new NVram("pram.bin", 256));

    // ADB bus is driven by Cuda
    this->adb_bus = std::unique_ptr<ADB_Bus> (new ADB_Bus());

    this->cuda_init();

    this->int_ctrl = nullptr;
}

int ViaCuda::device_postinit()
{
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::VIA_CUDA);

    return 0;
}

void ViaCuda::cuda_init() {
    this->old_tip     = 0;
    this->old_byteack = 0;
    this->treq        = 1;
    this->in_count    = 0;
    this->out_count   = 0;
    this->poll_rate   = 11;
}

uint8_t ViaCuda::read(int reg) {
    /* reading from some VIA registers triggers special actions */
    switch (reg & 0xF) {
    case VIA_B:
        return (this->via_regs[VIA_B]);
    case VIA_A:
    case VIA_ANH:
        LOG_F(WARNING, "Attempted read from VIA Port A!");
        break;
    case VIA_IER:
        return (this->_via_ier | 0x80); // bit 7 always reads as "1"
    case VIA_IFR:
        return this->_via_ifr;
    case VIA_T1CL:
        this->_via_ifr &= ~VIA_IF_T1;
        update_irq();
        return this->calc_counter_val(this->t1_counter, this->t1_start_time) & 0xFFU;
    case VIA_T1CH:
        return this->calc_counter_val(this->t1_counter, this->t1_start_time) >> 8;
    case VIA_T2CL:
        this->_via_ifr &= ~VIA_IF_T2;
        update_irq();
        return this->calc_counter_val(this->t2_counter, this->t2_start_time) & 0xFFU;
    case VIA_T2CH:
        return this->calc_counter_val(this->t2_counter, this->t2_start_time) >> 8;
    case VIA_SR:
        this->_via_ifr &= ~VIA_IF_SR;
        update_irq();
        break;
    }

    return (this->via_regs[reg & 0xF]);
}

void ViaCuda::write(int reg, uint8_t value) {
    this->via_regs[reg & 0xF] = value;

    switch (reg & 0xF) {
    case VIA_B:
        write(value);
        break;
    case VIA_A:
    case VIA_ANH:
        LOG_F(WARNING, "Attempted write to VIA Port A!");
        break;
    case VIA_DIRB:
        LOG_F(9, "VIA_DIRB = 0x%X", value);
        break;
    case VIA_DIRA:
        LOG_F(9, "VIA_DIRA = 0x%X", value);
        break;
    case VIA_PCR:
        LOG_F(9, "VIA_PCR = 0x%X", value);
        break;
    case VIA_ACR:
        LOG_F(9, "VIA_ACR = 0x%X", value);
        break;
    case VIA_IFR:
        // for each "1" in value clear the corresponding flags
        this->_via_ifr &= ~value;
        update_irq();
        break;
    case VIA_IER:
        if (value & 0x80) {
            this->_via_ier |= value & 0x7F;
        } else {
            this->_via_ier &= ~value;
        }
        update_irq();
        print_enabled_ints();
        break;
    case VIA_T1CH:
        if (this->via_regs[VIA_ACR] & 0xC0) {
            ABORT_F("Unsupported VIA T1 mode, ACR=0x%X", this->via_regs[VIA_ACR]);
        }
        // cancel active T1 timer task
        if (this->t1_active) {
            TimerManager::get_instance()->cancel_timer(this->t1_timer_id);
            this->t1_active = false;
        }
        // clear T1 flag in IFR
        this->_via_ifr &= ~VIA_IF_T1;
        update_irq();
        // load initial value into counter 1
        this->t1_counter = (value << 8) | this->via_regs[VIA_T1CL];
        // TODO: delay for one phase 2 clock
        // sample current vCPU time and remember it
        this->t1_start_time = TimerManager::get_instance()->current_time_ns();
        // set up timout timer for T1
        this->t1_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            static_cast<uint64_t>(this->via_clk_dur * (this->t1_counter + 3) + 0.5f),
            [this]() {
                this->assert_t1_int();
            }
        );
        this->t1_active = true;
        break;
    case VIA_T2CH:
        if (this->via_regs[VIA_ACR] & 0x20) {
            ABORT_F("VIA T2 pulse count mode not supported!");
        }
        // cancel active T2 timer task
        if (this->t2_active) {
            TimerManager::get_instance()->cancel_timer(this->t2_timer_id);
            this->t2_active = false;
        }
        // clear T2 flag in IFR
        this->_via_ifr &= ~VIA_IF_T2;
        update_irq();
        // load initial value into counter 2
        this->t2_counter = (value << 8) | this->via_regs[VIA_T2CL];
        // TODO: delay for one phase 2 clock
        // sample current vCPU time and remember it
        this->t2_start_time = TimerManager::get_instance()->current_time_ns();
        // set up timeout timer for T2
        this->t2_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            static_cast<uint64_t>(this->via_clk_dur * (this->t2_counter + 3) + 0.5f),
            [this]() {
                this->assert_t2_int();
            }
        );
        this->t2_active = true;
        break;
    case VIA_SR:
        this->_via_ifr &= ~VIA_IF_SR;
        update_irq();
        break;
    }
}

uint16_t ViaCuda::calc_counter_val(const uint16_t last_val, const uint64_t& last_time)
{
    // calcualte current counter value based on elapsed time and timer frequency
    uint64_t cur_time = TimerManager::get_instance()->current_time_ns();
    uint32_t diff = (cur_time - last_time) / this->via_clk_dur;
    return last_val - diff;
}

void ViaCuda::print_enabled_ints() {
    const char* via_int_src[] = {"CA2", "CA1", "SR", "CB2", "CB1", "T2", "T1"};

    for (int i = 0; i < 7; i++) {
        if (this->_via_ier & (1 << i))
            LOG_F(INFO, "VIA %s interrupt enabled", via_int_src[i]);
    }
}

inline void ViaCuda::update_irq() {
    uint8_t new_irq = !!(this->_via_ifr & this->_via_ier & 0x7F);
    this->_via_ifr  = (this->_via_ifr & 0x7F) | (new_irq << 7);
    if (new_irq != this->irq) {
        this->irq = new_irq;
        this->int_ctrl->ack_int(this->irq_id, new_irq);
    }
}

inline bool ViaCuda::ready() {
    return ((this->via_regs[VIA_DIRB] & 0x38) == 0x30);
}

void ViaCuda::assert_sr_int() {
    this->sr_timer_on = false;
    this->_via_ifr |= VIA_IF_SR;
    update_irq();
}

void ViaCuda::assert_t1_int() {
    this->_via_ifr |= VIA_IF_T1;
    this->t1_active = false;
    update_irq();
}

void ViaCuda::assert_t2_int() {
    this->_via_ifr |= VIA_IF_T2;
    this->t2_active = false;
    update_irq();
}

void ViaCuda::assert_ctrl_line(ViaLine line)
{
    switch (line) {
    case ViaLine::CA1:
        this->_via_ifr |= VIA_IF_CA1;
        break;
    case ViaLine::CA2:
        this->_via_ifr |= VIA_IF_CA2;
        break;
    case ViaLine::CB1:
        this->_via_ifr |= VIA_IF_CB1;
        break;
    case ViaLine::CB2:
        this->_via_ifr |= VIA_IF_CB1;
        break;
    default:
        ABORT_F("Assertion of unknown VIA line requested!");
    }
    update_irq();
}

void ViaCuda::schedule_sr_int(uint64_t timeout_ns) {
    if (this->sr_timer_on) {
        TimerManager::get_instance()->cancel_timer(this->sr_timer_id);
        this->sr_timer_on = false;
    }
    this->sr_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        timeout_ns,
        [this]() { this->assert_sr_int(); });
    this->sr_timer_on = true;
}

void ViaCuda::write(uint8_t new_state) {
    if (!ready()) {
        LOG_F(WARNING, "Cuda not ready!");
        return;
    }

    int new_tip     = !!(new_state & CUDA_TIP);
    int new_byteack = !!(new_state & CUDA_BYTEACK);

    /* return if there is no state change */
    if (new_tip == this->old_tip && new_byteack == this->old_byteack)
        return;

    LOG_F(9, "Cuda state changed!");

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
            LOG_F(9, "Cuda: enter sync state");
            this->via_regs[VIA_B] &= ~CUDA_TREQ; /* assert TREQ */
            this->treq      = 0;
            this->in_count  = 0;
            this->out_count = 0;
        }

        // send dummy byte as idle acknowledge or attention
        //assert_sr_int();
        schedule_sr_int(USECS_TO_NSECS(61));
    } else {
        if (this->via_regs[VIA_ACR] & 0x10) { /* data transfer: Host --> Cuda */
            if (this->in_count < 16) {
                this->in_buf[this->in_count++] = this->via_regs[VIA_SR];
                // tell the system we've read the byte after 71 usecs
                schedule_sr_int(USECS_TO_NSECS(71));
                //assert_sr_int();
            } else {
                LOG_F(WARNING, "Cuda input buffer too small. Truncating data!");
            }
        } else { /* data transfer: Cuda --> Host */
            (this->*out_handler)();
            //assert_sr_int();
            // tell the system we've written next byte after 88 usecs
            schedule_sr_int(USECS_TO_NSECS(88));
        }
    }
}

/* sends zeros to host ad infinitum */
void ViaCuda::null_out_handler() {
    this->via_regs[VIA_SR] = 0;
}

/* sends PRAM content to host ad infinitum */
void ViaCuda::pram_out_handler()
{
    this->via_regs[VIA_SR] = this->pram_obj->read_byte(this->cur_pram_addr++);
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
        LOG_F(ERROR, "Cuda: invalid packet (too few data)!");
        error_response(CUDA_ERR_BAD_SIZE);
        return;
    }

    switch (this->in_buf[0]) {
    case CUDA_PKT_ADB:
        LOG_F(9, "Cuda: ADB packet received \n");
        process_adb_command(this->in_buf[1], this->in_count - 2);
        break;
    case CUDA_PKT_PSEUDO:
        LOG_F(9, "Cuda: pseudo command packet received");
        LOG_F(9, "Command: 0x%X", this->in_buf[1]);
        LOG_F(9, "Data count: %d", this->in_count);
        for (int i = 0; i < this->in_count; i++) {
            LOG_F(9, "0x%X ,", this->in_buf[i]);
        }
        pseudo_command(this->in_buf[1], this->in_count - 2);
        break;
    default:
        LOG_F(ERROR, "Cuda: unsupported packet type = %d", this->in_buf[0]);
        error_response(CUDA_ERR_BAD_PKT);
    }
}

void ViaCuda::process_adb_command(uint8_t cmd_byte, int data_count) {
    int adb_dev = cmd_byte >> 4;    // 2 for keyboard, 3 for mouse
    int cmd     = cmd_byte & 0xF;

    if (!cmd) {
        LOG_F(9, "Cuda: ADB SendReset command requested");
        response_header(CUDA_PKT_ADB, 0);
    } else if (cmd == 1) {
        LOG_F(9, "Cuda: ADB Flush command requested");
        response_header(CUDA_PKT_ADB, 0);
    } else if ((cmd & 0xC) == 8) {
        LOG_F(9, "Cuda: ADB Listen command requested");
        int adb_reg = cmd_byte & 0x3;
        if (adb_bus->listen(adb_dev, adb_reg)) {
            response_header(CUDA_PKT_ADB, 0);
            for (int data_ptr = 0; data_ptr < adb_bus->get_output_len(); data_ptr++) {
                this->in_buf[(2 + data_ptr)] = adb_bus->get_output_byte(data_ptr);
            }
        } else {
            response_header(CUDA_PKT_ADB, 2);
        }
    } else if ((cmd & 0xC) == 0xC) {
        LOG_F(9, "Cuda: ADB Talk command requested");
        response_header(CUDA_PKT_ADB, 0);
        int adb_reg = cmd_byte & 0x3;
        if (adb_bus->talk(adb_dev, adb_reg, this->in_buf[2])) {
            response_header(CUDA_PKT_ADB, 0);
        } else {
            response_header(CUDA_PKT_ADB, 2);
        }
    } else {
        LOG_F(ERROR, "Cuda: unsupported ADB command 0x%X", cmd);
        error_response(CUDA_ERR_BAD_CMD);
    }
}

void ViaCuda::pseudo_command(int cmd, int data_count) {
    uint16_t    addr;
    int         i;

    switch (cmd) {
    case CUDA_START_STOP_AUTOPOLL:
        if (this->in_buf[2]) {
            LOG_F(INFO, "Cuda: autopoll started, rate: %d ms", this->poll_rate);
        } else {
            LOG_F(INFO, "Cuda: autopoll stopped");
        }
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    case CUDA_READ_MCU_MEM:
        addr = READ_WORD_BE_A(&this->in_buf[2]);
        response_header(CUDA_PKT_PSEUDO, 0);
        // if starting address is within PRAM region
        // prepare to transfer PRAM content, othwesise we will send zeroes
        if (addr >= CUDA_PRAM_START && addr <= CUDA_PRAM_END) {
            this->cur_pram_addr    = addr - CUDA_PRAM_START;
            this->next_out_handler = &ViaCuda::pram_out_handler;
        } else if (addr >= CUDA_ROM_START) {
            // HACK: Cuda ROM dump requsted so let's partially fake it
            this->out_buf[3] = 0; // empty copyright string
            WRITE_WORD_BE_A(&this->out_buf[4], 0x0019U);
            WRITE_WORD_BE_A(&this->out_buf[6], CUDA_FW_VERSION_MAJOR);
            WRITE_WORD_BE_A(&this->out_buf[8], CUDA_FW_VERSION_MINOR);
            this->out_count += 7;
        }
        this->is_open_ended = true;
        break;
    case CUDA_GET_REAL_TIME:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->out_buf[2] = (uint8_t)((this->real_time >> 24) & 0xFF);
        this->out_buf[3] = (uint8_t)((this->real_time >> 16) & 0xFF);
        this->out_buf[4] = (uint8_t)((this->real_time >> 8) & 0xFF);
        this->out_buf[5] = (uint8_t)((this->real_time) & 0xFF);
        break;
    case CUDA_WRITE_MCU_MEM:
        addr = READ_WORD_BE_A(&this->in_buf[2]);
        // if addr is inside PRAM, update PRAM with data from in_buf
        // otherwise, ignore data in in_buf
        if (addr >= CUDA_PRAM_START && addr <= CUDA_PRAM_END) {
            for (i = 0; i < this->in_count - 4; i++) {
                this->pram_obj->write_byte((addr - CUDA_PRAM_START + i) & 0xFF,
                                            this->in_buf[4+i]);
            }
        }
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    case CUDA_READ_PRAM:
        addr = READ_WORD_BE_A(&this->in_buf[2]);
        if (addr <= 0xFF) {
            response_header(CUDA_PKT_PSEUDO, 0);
            // this command is open-ended so set up the corresponding context
            this->cur_pram_addr    = addr;
            this->next_out_handler = &ViaCuda::pram_out_handler;
            this->is_open_ended    = true;
        } else {
            error_response(CUDA_ERR_BAD_PAR);
        }
        break;
    case CUDA_SET_REAL_TIME:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->real_time = ((uint32_t)in_buf[2]) >> 24;
        this->real_time += ((uint32_t)in_buf[3]) >> 16;
        this->real_time += ((uint32_t)in_buf[4]) >> 8;
        this->real_time += ((uint32_t)in_buf[5]);
        break;
    case CUDA_WRITE_PRAM:
        addr = READ_WORD_BE_A(&this->in_buf[2]);
        if (addr <= 0xFF) {
            // transfer data from in_buf to PRAM
            for (i = 0; i < this->in_count - 4; i++) {
                this->pram_obj->write_byte((addr + i) & 0xFF, this->in_buf[4+i]);
            }
            response_header(CUDA_PKT_PSEUDO, 0);
        } else {
            error_response(CUDA_ERR_BAD_PAR);
        }
        break;
    case CUDA_FILE_SERVER_FLAG:
        response_header(CUDA_PKT_PSEUDO, 0);
        if (this->in_buf[2]) {
            LOG_F(INFO, "Cuda: File server flag on");
            this->file_server = true;
        } else {
            LOG_F(INFO, "Cuda: File server flag off");
            this->file_server = false;
        }
        break;
    case CUDA_SET_AUTOPOLL_RATE:
        this->poll_rate = this->in_buf[2];
        LOG_F(INFO, "Cuda: autopoll rate set to %d ms", this->poll_rate);
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    case CUDA_GET_AUTOPOLL_RATE:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->out_buf[3] = this->poll_rate;
        this->out_count++;
        break;
    case CUDA_SET_DEVICE_LIST:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->device_mask = ((uint16_t)in_buf[2]) >> 8;
        this->device_mask += ((uint16_t)in_buf[3]);
        break;
    case CUDA_GET_DEVICE_LIST:
        response_header(CUDA_PKT_PSEUDO, 0);
        this->out_buf[2] = (uint8_t)((this->device_mask >> 8) & 0xFF);
        this->out_buf[3] = (uint8_t)((this->device_mask) & 0xFF);
        break;
    case CUDA_ONE_SECOND_MODE:
        LOG_F(INFO, "Cuda: One Second Interrupt - Byte Sent: %d", this->in_buf[2]);
        response_header(CUDA_PKT_PSEUDO, 0);
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
        LOG_F(INFO, "Cuda: send %d to PB0", (int)(this->in_buf[2]));
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    case CUDA_WARM_START:
    case CUDA_POWER_DOWN:
    case CUDA_MONO_STABLE_RESET:
    case CUDA_RESTART_SYSTEM:
        /* really kludge temp code */
        LOG_F(INFO, "Cuda: Restart/Shutdown signal sent with command 0x%x! \n", cmd);
        //exit(0);
        break;
    default:
        LOG_F(ERROR, "Cuda: unsupported pseudo command 0x%X", cmd);
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
        LOG_F(WARNING, "Unsupported I2C device 0x%X", dev_addr);
        error_response(CUDA_ERR_I2C);
        return;
    }

    /* send data to the target I2C device until there is no more data to send
       or the target device doesn't acknowledge that indicates an error */
    for (int i = 0; i < in_bytes; i++) {
        if (!this->send_byte(dev_addr, in_buf[i])) {
            LOG_F(WARNING, "NO_ACK during sending, device 0x%X", dev_addr);
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
        LOG_F(ERROR, "Combined I2C: dev_addr mismatch!");
        error_response(CUDA_ERR_I2C);
        return;
    }

    dev_addr >>= 1; /* strip RD/WR bit */

    if (!this->start_transaction(dev_addr)) {
        LOG_F(WARNING, "Unsupported I2C device 0x%X", dev_addr);
        error_response(CUDA_ERR_I2C);
        return;
    }

    if (!this->send_subaddress(dev_addr, sub_addr)) {
        LOG_F(WARNING, "NO_ACK while sending subaddress, device 0x%X", dev_addr);
        error_response(CUDA_ERR_I2C);
        return;
    }

    /* send data to the target I2C device until there is no more data to send
       or the target device doesn't acknowledge that indicates an error */
    for (int i = 0; i < in_bytes; i++) {
        if (!this->send_byte(dev_addr, in_buf[i])) {
            LOG_F(WARNING, "NO_ACK during sending, device 0x%X", dev_addr);
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
