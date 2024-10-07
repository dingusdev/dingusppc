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

/** High-level VIA-CUDA combo device emulation.
 */

#include <core/hostevents.h>
#include <core/timermanager.h>
#include <devices/common/adb/adbbus.h>
#include <cpu/ppc/ppcemu.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/viacuda.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cinttypes>
#include <string>
#include <vector>

using namespace std;

ViaCuda::ViaCuda() {
    this->name = "ViaCuda";

    supports_types(HWCompType::ADB_HOST | HWCompType::I2C_HOST);

    // VIA reset clears all internal registers to logic 0
    // except timers/counters and the shift register
    // as stated in the 6522 datasheet
    this->via_porta = 0;
    this->via_portb = 0;
    this->via_ddrb  = 0;
    this->via_ddra  = 0;
    this->via_acr   = 0;
    this->via_pcr   = 0;

    // load maximum value into the timer registers for safety
    // (not prescribed in the 6522 datasheet)
    this->via_t1ll  = 0xFF;
    this->via_t1lh  = 0xFF;
    this->via_t2cl  = 0xFF;

    this->_via_ifr  = 0; // all flags cleared
    this->_via_ier  = 0; // all interrupts disabled

    // intialize counters/timers
    this->t1_counter  = 0xFFFF;
    this->t2_counter  = 0xFFFF;

    // calculate VIA clock duration in ns
    this->via_clk_dur = 1.0f / VIA_CLOCK_HZ * NS_PER_SEC;

    // PRAM is part of Cuda
    this->pram_obj = std::unique_ptr<NVram> (new NVram("pram.bin", 256));

    // establish ADB bus connection
    this->adb_bus_obj = dynamic_cast<AdbBus*>(gMachineObj->get_comp_by_type(HWCompType::ADB_HOST));

    // autopoll handler will be called during post-processing of the host events
    EventManager::get_instance()->add_post_handler(this, &ViaCuda::autopoll_handler);

    this->cuda_init();

    this->int_ctrl = nullptr;

    std::tm tm = {
        .tm_sec  = 0,
        .tm_min  = 0,
        .tm_hour = 0,
        .tm_mday = 1,
        .tm_mon  = 1 - 1,
        .tm_year = 1904 - 1900,
        .tm_isdst = 1 // -1 = Use DST value from local time zone; 0 = not summer; 1 = is summer time
    };
    mac_epoch = std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

ViaCuda::~ViaCuda()
{
    if (this->sr_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->sr_timer_id);
        this->sr_timer_id = 0;
    }
    if (this->t1_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->t1_timer_id);
        this->t1_timer_id = 0;
    }
    if (this->t2_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->t2_timer_id);
        this->t2_timer_id = 0;
    }
    if (this->treq_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->treq_timer_id);
        this->treq_timer_id = 0;
    }
}

int ViaCuda::device_postinit()
{
    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::VIA_CUDA);

    return 0;
}

void ViaCuda::cuda_init() {
    this->old_tip     = 1;
    this->old_byteack = 1;
    this->treq        = 1;
    this->in_count    = 0;
    this->out_count   = 0;
    this->poll_rate   = 11;
}

uint8_t ViaCuda::read(int reg) {
    uint8_t value;

    switch (reg & 0xF) {
    case VIA_B:
        return this->via_portb; //(this->via_portb & ~this->via_ddrb) | this->last_orb;
    case VIA_A:
    case VIA_ANH:
        LOG_F(WARNING, "Attempted read from VIA Port A!");
        return this->via_porta;
    case VIA_DIRB:
        return this->via_ddrb;
    case VIA_DIRA:
        return this->via_ddra;
    case VIA_T1CL:
        this->_via_ifr &= ~VIA_IF_T1;
        update_irq();
        return this->calc_counter_val(this->t1_counter, this->t1_start_time) & 0xFFU;
    case VIA_T1CH:
        return this->calc_counter_val(this->t1_counter, this->t1_start_time) >> 8;
    case VIA_T1LL:
        return this->via_t1ll;
    case VIA_T1LH:
        return this->via_t1lh;
    case VIA_T2CL:
        this->_via_ifr &= ~VIA_IF_T2;
        update_irq();
        return this->calc_counter_val(this->t2_counter, this->t2_start_time) & 0xFFU;
    case VIA_T2CH:
        return this->calc_counter_val(this->t2_counter, this->t2_start_time) >> 8;
    case VIA_SR:
        value = this->via_sr;
        this->_via_ifr &= ~VIA_IF_SR;
        update_irq();
        return value;
    case VIA_ACR:
        return this->via_acr;
    case VIA_PCR:
        return this->via_pcr;
    case VIA_IFR:
        return this->_via_ifr;
    case VIA_IER:
        return (this->_via_ier | 0x80); // bit 7 always reads as "1"
    }

    return 0; // should never happen!
}

void ViaCuda::write(int reg, uint8_t value) {
    switch (reg & 0xF) {
    case VIA_B:
        this->last_orb = value & this->via_ddrb;
        this->via_portb = (this->via_portb & ~this->via_ddrb) | this->last_orb;
        // ensure the proper VIA configuration before calling Cuda
        if ((this->via_ddrb & 0x38) == 0x30)
            this->write(this->via_portb);
        break;
    case VIA_A:
    case VIA_ANH:
        this->via_porta = value;
        LOG_F(WARNING, "Attempted write to VIA Port A!");
        break;
    case VIA_DIRB:
        this->via_ddrb = value;
        LOG_F(9, "VIA_DIRB = 0x%X", value);
        break;
    case VIA_DIRA:
        this->via_ddra = value;
        LOG_F(9, "VIA_DIRA = 0x%X", value);
        break;
    case VIA_T1CL:
        this->via_t1cl = value;
        break;
    case VIA_T1CH:
        if (this->via_acr & 0xC0) {
            ABORT_F("Unsupported VIA T1 mode, ACR=0x%X", this->via_acr);
        }
        // cancel active T1 timer task
        if (this->t1_timer_id) {
            TimerManager::get_instance()->cancel_timer(this->t1_timer_id);
            this->t1_timer_id = 0;
        }
        // clear T1 flag in IFR
        this->_via_ifr &= ~VIA_IF_T1;
        update_irq();
        // load initial value into counter 1
        this->t1_counter = (value << 8) | this->via_t1cl;
        // TODO: delay for one phase 2 clock
        // sample current vCPU time and remember it
        this->t1_start_time = TimerManager::get_instance()->current_time_ns();
        // set up timout timer for T1
        this->t1_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            static_cast<uint64_t>(this->via_clk_dur * (this->t1_counter + 3) + 0.5f),
            [this]() {
                this->t1_timer_id = 0;
                this->assert_t1_int();
            }
        );
        break;
    case VIA_T1LL:
        this->via_t1ll = value;
        break;
    case VIA_T1LH:
        this->via_t1lh = value;
        break;
    case VIA_T2CL:
        this->via_t2cl = value;
        break;
    case VIA_T2CH:
        if (this->via_acr & 0x20) {
            ABORT_F("VIA T2 pulse count mode not supported!");
        }
        // cancel active T2 timer task
        if (this->t2_timer_id) {
            TimerManager::get_instance()->cancel_timer(this->t2_timer_id);
            this->t2_timer_id = 0;
        }
        // clear T2 flag in IFR
        this->_via_ifr &= ~VIA_IF_T2;
        update_irq();
        // load initial value into counter 2
        this->t2_counter = (value << 8) | this->via_t2cl;
        // TODO: delay for one phase 2 clock
        // sample current vCPU time and remember it
        this->t2_start_time = TimerManager::get_instance()->current_time_ns();
        // set up timeout timer for T2
        this->t2_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            static_cast<uint64_t>(this->via_clk_dur * (this->t2_counter + 3) + 0.5f),
            [this]() {
                this->t2_timer_id = 0;
                this->assert_t2_int();
            }
        );
        break;
    case VIA_SR:
        this->via_sr = value;
        this->_via_ifr &= ~VIA_IF_SR;
        update_irq();
        break;
    case VIA_ACR:
        this->via_acr = value;
        LOG_F(9, "VIA_ACR = 0x%X", value);
        break;
    case VIA_PCR:
        this->via_pcr = value;
        LOG_F(9, "VIA_PCR = 0x%X", value);
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
    }
}

uint16_t ViaCuda::calc_counter_val(const uint16_t last_val, const uint64_t& last_time)
{
    // calculate current counter value based on elapsed time and timer frequency
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

void ViaCuda::update_irq()
{
    uint8_t active_ints = this->_via_ifr & this->_via_ier & 0x7F;
    if (active_ints != this->old_ifr) {
        uint8_t new_irq = !!active_ints;
        this->_via_ifr  = (active_ints) | (new_irq << 7);
        this->old_ifr = active_ints;
        LOG_F(6, "VIA: assert IRQ, IFR=0x%02X", this->_via_ifr);
        this->int_ctrl->ack_int(this->irq_id, new_irq);
    }
}

void ViaCuda::assert_sr_int() {
    this->_via_ifr |= VIA_IF_SR;
    update_irq();
}

void ViaCuda::assert_t1_int() {
    this->_via_ifr |= VIA_IF_T1;
    update_irq();
}

void ViaCuda::assert_t2_int() {
    this->_via_ifr |= VIA_IF_T2;
    update_irq();
}

#ifdef DEBUG_CPU_INT
void ViaCuda::assert_int(uint8_t flags) {
    this->_via_ifr |= (flags & 0x7F);
    update_irq();
}
#endif

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
    if (this->sr_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->sr_timer_id);
        this->sr_timer_id = 0;
    }
    this->sr_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        timeout_ns,
        [this]() {
            this->sr_timer_id = 0;
            this->assert_sr_int();
        }
    );
}

void ViaCuda::write(uint8_t new_state) {
    int new_tip     = !!(new_state & CUDA_TIP);
    int new_byteack = !!(new_state & CUDA_BYTEACK);

    // return if there is no state change
    if (new_tip == this->old_tip && new_byteack == this->old_byteack)
        return;

    this->old_tip     = new_tip;
    this->old_byteack = new_byteack;

    if (new_tip) {
        if (new_byteack) {
            this->via_portb |= CUDA_TREQ; // negate TREQ
            this->treq = 1;

            if (this->in_count) {
                process_packet();

                // start response transaction
                this->treq_timer_id = TimerManager::get_instance()->add_oneshot_timer(
                    USECS_TO_NSECS(13), // delay TREQ assertion for New World
                    [this]() {
                        this->via_portb &= ~CUDA_TREQ; // assert TREQ
                        this->treq = 0;
                        this->treq_timer_id = 0;
                });
            }

            this->in_count = 0;
        } else {
            LOG_F(9, "Cuda: enter sync state");
            this->via_portb &= ~CUDA_TREQ; // assert TREQ
            this->treq      = 0;
            this->in_count  = 0;
            this->out_count = 0;
        }

        // send dummy byte as idle acknowledge or attention
        schedule_sr_int(USECS_TO_NSECS(61));
    } else {
        if (this->via_acr & 0x10) { // data transfer: Host --> Cuda
            if (this->in_count < sizeof(this->in_buf)) {
                this->in_buf[this->in_count++] = this->via_sr;
                // tell the system we've read the byte after 71 usecs
                schedule_sr_int(USECS_TO_NSECS(71));
            } else {
                LOG_F(WARNING, "Cuda input buffer too small. Truncating data!");
            }
        } else { // data transfer: Cuda --> Host
            (this->*out_handler)();
            // tell the system we've written next byte after 88 usecs
            schedule_sr_int(USECS_TO_NSECS(88));
        }
    }
}

/* sends zeros to host ad infinitum */
void ViaCuda::null_out_handler() {
    this->via_sr = 0;
}

/* sends PRAM content to host ad infinitum */
void ViaCuda::pram_out_handler()
{
    this->via_sr = this->pram_obj->read_byte(this->cur_pram_addr++);
}

/* sends data from out_buf until exhausted, then switches to next_out_handler */
void ViaCuda::out_buf_handler() {
    if (this->out_pos < this->out_count) {
        this->via_sr = this->out_buf[this->out_pos++];
        if (!this->is_open_ended && this->out_pos >= this->out_count) {
            // tell the host this will be the last byte
            this->via_portb |= CUDA_TREQ; // negate TREQ
            this->treq = 1;
        }
    } else if (this->is_open_ended) {
        this->out_handler      = this->next_out_handler;
        this->next_out_handler = &ViaCuda::null_out_handler;
        (this->*out_handler)();
    } else {
        this->out_count = 0;
        this->via_portb |= CUDA_TREQ; // negate TREQ
        this->treq = 1;
    }
}

void ViaCuda::response_header(uint32_t pkt_type, uint32_t pkt_flag) {
    this->out_buf[0]       = pkt_type;
    this->out_buf[1]       = pkt_flag;
    this->out_buf[2]       = this->in_buf[1]; // copy original cmd
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
    this->out_buf[3]       = this->in_buf[1]; // copy original cmd
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
        LOG_F(9, "Cuda: ADB packet received");
        this->process_adb_command();
        break;
    case CUDA_PKT_PSEUDO:
        LOG_F(9, "Cuda: pseudo command packet received");
        LOG_F(9, "Command: 0x%X", this->in_buf[1]);
        LOG_F(9, "Data count: %d", this->in_count);
        for (int i = 0; i < this->in_count; i++) {
            LOG_F(9, "0x%X ,", this->in_buf[i]);
        }
        pseudo_command();
        break;
    default:
        LOG_F(ERROR, "Cuda: unsupported packet type = %d", this->in_buf[0]);
        error_response(CUDA_ERR_BAD_PKT);
    }
}

void ViaCuda::process_adb_command() {
    uint8_t adb_stat, output_size;

    adb_stat = this->adb_bus_obj->process_command(&this->in_buf[1],
                                                  this->in_count - 1);
    response_header(CUDA_PKT_ADB, adb_stat);
    output_size = this->adb_bus_obj->get_output_count();
    if (output_size) {
        std::memcpy(&this->out_buf[3], this->adb_bus_obj->get_output_buf(), output_size);
        this->out_count += output_size;
    }
}

void ViaCuda::autopoll_handler() {
    uint8_t poll_command = this->autopoll_enabled ? this->adb_bus_obj->poll() : 0;

    if (poll_command) {
        if (!this->old_tip || !this->treq) {
            LOG_F(WARNING, "Cuda transaction probably in progress");
        }

        // prepare autopoll packet
        response_header(CUDA_PKT_ADB, ADB_STAT_OK | ADB_STAT_AUTOPOLL);
        this->out_buf[2] = poll_command; // put the proper ADB command
        uint8_t output_size = this->adb_bus_obj->get_output_count();
        if (output_size) {
            std::memcpy(&this->out_buf[3], this->adb_bus_obj->get_output_buf(), output_size);
            this->out_count += output_size;
        }

        // assert TREQ
        this->via_portb &= ~CUDA_TREQ;
        this->treq = 0;

        // draw guest system's attention
        schedule_sr_int(USECS_TO_NSECS(30));
    } else if (this->one_sec_mode != 0) {
        uint32_t this_time = calc_real_time();
        if (this_time != this->last_time) {
            /*
                We'll send a time packet every 4
                seconds just in case we get out of
                sync.
            */
            bool send_time = !(this->last_time & 3);
            if (send_time || this->one_sec_mode < 3) {
                response_header(CUDA_PKT_PSEUDO, 0);
                this->out_buf[2] = CUDA_GET_REAL_TIME;
                if (send_time || this->one_sec_mode == 1) {
                    uint32_t real_time = this_time + this->time_offset;
                    WRITE_DWORD_BE_U(&this->out_buf[3], real_time);
                    this->out_count = 7;
                }
            } else if (this->one_sec_mode == 3) {
                response_header(CUDA_PKT_TICK, 0);
                this->out_count = 1;
            }
            this->last_time = this_time;

            // assert TREQ
            this->via_portb &= ~CUDA_TREQ;
            this->treq = 0;

            // draw guest system's attention
            schedule_sr_int(USECS_TO_NSECS(30));
        }
    }
}

void ViaCuda::pseudo_command() {
    uint16_t    addr;
    int         i;
    int         cmd = this->in_buf[1];

    switch (cmd) {
    case CUDA_START_STOP_AUTOPOLL:
        if (this->in_buf[2]) {
            LOG_F(INFO, "Cuda: autopoll started, rate: %d ms", this->poll_rate);
            this->autopoll_enabled = true;
        } else {
            LOG_F(INFO, "Cuda: autopoll stopped");
            this->autopoll_enabled = false;
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
    case CUDA_GET_REAL_TIME: {
        response_header(CUDA_PKT_PSEUDO, 0);
        uint32_t this_time = this->calc_real_time();
        uint32_t real_time = this_time + this->time_offset;
        WRITE_DWORD_BE_U(&this->out_buf[3], real_time);
        this->out_count = 7;
        break;
    }
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
    case CUDA_SET_REAL_TIME: {
        response_header(CUDA_PKT_PSEUDO, 0);
        uint32_t real_time = this->calc_real_time();
        uint32_t new_time = READ_DWORD_BE_U(&in_buf[2]);
        this->time_offset = new_time - real_time;
        break;
    }
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
        LOG_F(INFO, "Cuda: One Second Interrupt Mode: %d", this->in_buf[2]);
        this->one_sec_mode = this->in_buf[2];
        response_header(CUDA_PKT_PSEUDO, 0);
        break;
    case CUDA_SET_POWER_MESSAGES:
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
    case CUDA_RESTART_SYSTEM:
        LOG_F(INFO, "Cuda: system restart");
        power_on = false;
        power_off_reason = po_restart;
        break;
    case CUDA_POWER_DOWN:
        LOG_F(INFO, "Cuda: system shutdown");
        power_on = false;
        power_off_reason = po_shut_down;
        break;
    case CUDA_WARM_START:
    case CUDA_MONO_STABLE_RESET:
        /* really kludge temp code */
        LOG_F(INFO, "Cuda: Restart/Shutdown signal sent with command 0x%x!", cmd);
        //exit(0);
        break;
    default:
        LOG_F(ERROR, "Cuda: unsupported pseudo command 0x%X", cmd);
        error_response(CUDA_ERR_BAD_CMD);
    }
}

uint32_t ViaCuda::calc_real_time() {
    auto end = std::chrono::system_clock::now();
    auto elapsed_systemclock = end - this->mac_epoch;
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed_systemclock);
    return uint32_t(elapsed_seconds.count());
}

/* sends data from the current I2C to host ad infinitum */
void ViaCuda::i2c_handler() {
    this->receive_byte(this->curr_i2c_addr, &this->via_sr);
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

static const vector<string> Cuda_Subdevices = {
    "AdbBus"
};

static const DeviceDescription ViaCuda_Descriptor = {
    ViaCuda::create, Cuda_Subdevices, {}
};

REGISTER_DEVICE(ViaCuda, ViaCuda_Descriptor);
