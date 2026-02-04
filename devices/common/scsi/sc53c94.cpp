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

/** @file NCR53C94/Am53CF94 SCSI controller emulation. */

#include <core/timermanager.h>
#include <devices/common/dmacore.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/scsi/sc53c94.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>
#include <cstring>

Sc53C94::Sc53C94(uint8_t chip_id, uint8_t my_id) : ScsiPhysDevice("SC53C94", my_id), DmaDevice()
{
    this->chip_id   = chip_id;
    this->my_bus_id = my_id;
    supports_types(HWCompType::SCSI_HOST | HWCompType::SCSI_DEV);
    reset_device();
}

int Sc53C94::device_postinit()
{
    ScsiBus* bus = dynamic_cast<ScsiBus*>(gMachineObj->get_comp_by_name("ScsiCurio"));
    if (bus) {
        bus->register_device(7, static_cast<ScsiPhysDevice*>(this));
        bus->attach_scsi_devices("");
    }

    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::SCSI_CURIO);

    return 0;
}

void Sc53C94::reset_device()
{
    // part-unique ID to be read using a magic sequence
    this->xfer_count = this->chip_id << 16;

    this->clk_factor   = 2;
    this->sel_timeout  = 0;
    this->is_initiator = true;

    // clear command FIFO
    this->cmd_fifo_pos = 0;

    // clear data FIFO
    this->data_fifo_pos = 0;
    this->data_fifo[0]  = 0;

    this->sync_period = 5;
    this->sync_offset = 0;

    this->cur_step = 0;
    this->seq_step = 0;

    this->status    &= STAT_PHASE_MASK; // reset doesn't affect bus phase bits
    this->int_status = 0;
}

uint8_t Sc53C94::read(uint8_t reg_offset)
{
    uint8_t bus_phase, int_flags;

    switch (reg_offset) {
    case Read::Reg53C94::Xfer_Cnt_LSB:
        return this->xfer_count & 0xFFU;
    case Read::Reg53C94::Xfer_Cnt_MSB:
        return (this->xfer_count >> 8) & 0xFFU;
    case Read::Reg53C94::FIFO:
        return this->fifo_pop();
    case Read::Reg53C94::Command:
        return this->cmd_fifo[0];
    case Read::Reg53C94::Status:
        if (this->config2 & CFG2_ENF) {
            static bool log_it = true;
            if (log_it) {
                LOG_F(WARNING, "%s: phase latch not implemented", this->name.c_str());
                log_it = false;
            }
            bus_phase = SCSI_CTRL_MSG; // use reserved bus phase
        } else
            bus_phase = bus_obj->test_ctrl_lines(SCSI_CTRL_MSG | SCSI_CTRL_CD | SCSI_CTRL_IO);
        return (this->status & 0xF8) | bus_phase;
    case Read::Reg53C94::Int_Status:
        int_flags = this->int_status;
        if (this->irq) {
            this->status &= ~(STAT_GE | STAT_PE | STAT_GCV);
            this->int_status = 0;
            this->seq_step = 0;
        }
        this->update_irq();
        return int_flags;
    case Read::Reg53C94::Seq_Step:
        return this->seq_step;
    case Read::Reg53C94::FIFO_Flags:
        return (this->cur_step << 5) | (this->data_fifo_pos & 0x1F);
    case Read::Reg53C94::Config_1:
        return this->config1;
    case Read::Reg53C94::Config_3:
        return this->config3;
    case Read::Reg53C94::Xfer_Cnt_Hi:
        if (this->config2 & CFG2_ENF) {
            return (this->xfer_count >> 16) & 0xFFU;
        }
        break;
    default:
        LOG_F(INFO, "%s: reading from register %d", this->name.c_str(), reg_offset);
    }
    return 0;
}

void Sc53C94::write(uint8_t reg_offset, uint8_t value)
{
    switch (reg_offset) {
    case Write::Reg53C94::Xfer_Cnt_LSB:
        this->set_xfer_count = (this->set_xfer_count & ~0xFFU) | value;
        break;
    case Write::Reg53C94::Xfer_Cnt_MSB:
        this->set_xfer_count = (this->set_xfer_count & ~0xFF00U) | (value << 8);
        break;
    case Write::Reg53C94::Command:
        update_command_reg(value);
        break;
    case Write::Reg53C94::FIFO:
        fifo_push(value);
        break;
    case Write::Reg53C94::Dest_Bus_ID:
        this->target_id = value & 7;
        break;
    case Write::Reg53C94::Sel_Timeout:
        this->sel_timeout = value;
        break;
    case Write::Reg53C94::Sync_Period:
        this->sync_period = value;
        break;
    case Write::Reg53C94::Sync_Offset:
        this->sync_offset = value;
        break;
    case Write::Reg53C94::Clock_Factor:
        this->clk_factor = value;
        break;
    case Write::Reg53C94::Config_1:
        if ((value & 7) != this->my_bus_id) {
            ABORT_F("%s: HBA bus ID mismatch!", this->name.c_str());
        }
        this->config1 = value;
        break;
    case Write::Reg53C94::Config_2:
        this->config2 = value;
        break;
    case Write::Reg53C94::Config_3:
        this->config3 = value;
        break;
    default:
        LOG_F(INFO, "%s: writing 0x%X to %d register", this->name.c_str(), value,
              reg_offset);
    }
}

uint16_t Sc53C94::pseudo_dma_read()
{
    uint16_t data_word;
    bool     is_done = false;

    if (this->data_fifo_pos >= 2) {
        // remove one word from FIFO
        data_word = (this->data_fifo[0] << 8) | this->data_fifo[1];
        this->data_fifo_pos -= 2;
        std::memmove(this->data_fifo, &this->data_fifo[2], this->data_fifo_pos);

        // update DMA status
        if (this->is_dma_cmd) {
            this->xfer_count -= 2;
            if (!this->xfer_count) {
                is_done = true;
                this->status |= STAT_TC; // signal zero transfer count
                this->cur_state = SeqState::XFER_END;
                this->sequencer();
            }
        }
    }
    else {
        LOG_F(ERROR, "SC53C94: FIFO underrun %d", data_fifo_pos);
        data_word = 0;
    }

    // see if we need to refill FIFO
    if (!this->data_fifo_pos && !is_done) {
        this->sequencer();
    }

    return data_word;
}

void Sc53C94::pseudo_dma_write(uint16_t data) {
    this->fifo_push((data >> 8) & 0xFFU);
    this->fifo_push(data & 0xFFU);

    // update DMA status
    if (this->is_dma_cmd) {
        this->xfer_count -= 2;
        if (!this->xfer_count) {
            this->status |= STAT_TC; // signal zero transfer count
            //this->cur_state = SeqState::XFER_END;
            this->sequencer();
        }
    }
}

void Sc53C94::update_command_reg(uint8_t cmd)
{
    if (this->on_reset && (cmd & CMD_OPCODE) != CMD_NOP) {
        LOG_F(WARNING, "%s: command register blocked after RESET!", this->name.c_str());
        return;
    }

    // NOTE: Reset Device (chip), Reset Bus and DMA Stop commands execute
    // immediately while all others are placed into the command FIFO
    switch (cmd & CMD_OPCODE) {
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
        LOG_F(ERROR, "%s: the top of the command FIFO overwritten!", this->name.c_str());
        this->status |= STAT_GE; // signal IOE/Gross Error
    }
}

void Sc53C94::exec_command()
{
    uint8_t cmd = this->cur_cmd = this->cmd_fifo[0] & CMD_OPCODE;

    this->is_dma_cmd = !!(this->cmd_fifo[0] & CMD_ISDMA);

    if (this->is_dma_cmd) {
        if (this->config2 & CFG2_ENF) { // extended mode: 24-bit
            this->xfer_count = this->set_xfer_count & 0xFFFFFFUL;
        } else { // standard mode: 16-bit
            this->xfer_count = this->set_xfer_count & 0xFFFFUL;
            if (!this->xfer_count) {
                this->xfer_count = 65536;
            }
        }
    }

    this->cmd_steps = nullptr; // assume a single-step command for now

    // simple commands will be executed immediately
    // complex commands will be broken into multiple steps
    // and handled by the sequencer
    switch (cmd) {
    case CMD_NOP:
        this->on_reset = false; // unblock the command register
        exec_next_command();
        break;
    case CMD_CLEAR_FIFO:
        this->data_fifo_pos = 0; // set the bottom of the data FIFO to zero
        this->data_fifo[0] = 0;
        exec_next_command();
        break;
    case CMD_RESET_DEVICE:
        reset_device();
        this->on_reset = true; // block the command register
        return;
    case CMD_RESET_BUS:
        LOG_F(INFO, "%s: resetting SCSI bus...", this->name.c_str());
        // assert RST line
        this->bus_obj->assert_ctrl_line(this->my_bus_id, SCSI_CTRL_RST);
        // release RST line after 25 us
        if (my_timer_id) {
            TimerManager::get_instance()->cancel_timer(this->my_timer_id);
            my_timer_id = 0;
        }
        my_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            USECS_TO_NSECS(25),
            [this]() {
                my_timer_id = 0;
                this->bus_obj->release_ctrl_line(this->my_bus_id, SCSI_CTRL_RST);
        });
        if (!(config1 & CFG1_DISR)) {
            LOG_F(INFO, "%s: reset interrupt issued", this->name.c_str());
            this->int_status = INTSTAT_SRST;
            this->update_irq();
        }
        exec_next_command();
        break;
    case CMD_XFER:
        if (!this->is_initiator) {
            // clear command FIFO
            this->cmd_fifo_pos = 0;
            this->int_status = INTSTAT_ICMD;
            this->update_irq();
        } else {
            this->cur_state = SeqState::XFER_BEGIN;
            this->sequencer();
        }
        break;
    case CMD_COMPLETE_STEPS:
        if (this->bus_obj->current_phase() != ScsiPhase::STATUS) {
            ABORT_F("%s: complete steps only works in the STATUS phase", this->name.c_str());
        }
        this->cur_state = SeqState::RCV_STATUS;
        this->sequencer();
        break;
    case CMD_MSG_ACCEPTED:
        // Don't release ACK if ATN is asserted.
        // Executing this command with ATN true means that
        // the initiator wants to reject the current message.
        // That should be recognized and handled by the target.
        if (!this->bus_obj->test_ctrl_lines(SCSI_CTRL_ATN))
            this->bus_obj->release_ctrl_line(this->my_bus_id, SCSI_CTRL_ACK);
        if (this->is_initiator) {
            this->bus_obj->target_next_step();
        }
        this->int_status |= INTSTAT_SR;
        this->update_irq();
        exec_next_command();
        break;
    case CMD_XFER_PAD_BYTES:
        if (this->bus_obj->current_phase() != ScsiPhase::COMMAND)
            ABORT_F("%s: unsupported phase %d in CMD_XFER_PAD_BYTES",
                    this->name.c_str(), this->bus_obj->current_phase());
        std::memset(this->data_fifo, 0, DATA_FIFO_MAX);
        // FIXME: does the non-DMA version of this command use the transfer counter?
        this->data_fifo_pos = std::min((int)this->set_xfer_count, DATA_FIFO_MAX);
        this->cur_state = SeqState::SEND_CMD;
        this->sequencer();
        if (this->bus_obj->current_phase() != ScsiPhase::COMMAND) {
            this->int_status |= INTSTAT_SR;
            this->update_irq();
            exec_next_command();
        }
        break;
    case CMD_RESET_ATN:
        this->bus_obj->release_ctrl_line(this->my_bus_id, SCSI_CTRL_ATN);
        exec_next_command();
        break;
    case CMD_SELECT_NO_ATN:
        static SeqDesc * sel_no_atn_desc = new SeqDesc[2]{
            {2, ScsiPhase::COMMAND, SeqState::SEND_CMD,     INTSTAT_SR | INTSTAT_SO},
            {4, -1,                 SeqState::CMD_COMPLETE, INTSTAT_SR | INTSTAT_SO},
        };
        this->seq_step = this->cur_step = 0;
        this->cmd_steps = sel_no_atn_desc;
        this->cur_state = SeqState::BUS_FREE;
        this->sequencer();
        LOG_F(9, "%s: SELECT W/O ATN command started", this->name.c_str());
        break;
    case CMD_SELECT_WITH_ATN:
        static SeqDesc * sel_with_atn_desc = new SeqDesc[3]{
            {0, ScsiPhase::MESSAGE_OUT, SeqState::SEND_MSG,     INTSTAT_SR | INTSTAT_SO},
            {2, ScsiPhase::COMMAND,     SeqState::SEND_CMD,     INTSTAT_SR | INTSTAT_SO},
            {4, -1,                     SeqState::CMD_COMPLETE, INTSTAT_SR | INTSTAT_SO},
        };
        this->seq_step = this->cur_step = 0;
        this->bytes_out = 1; // set message length
        this->cmd_steps = sel_with_atn_desc;
        this->cur_state = SeqState::BUS_FREE;
        this->sequencer();
        LOG_F(9, "%s: SELECT WITH ATN command started", this->name.c_str());
        break;
    case CMD_SELECT_WITH_ATN_AND_STOP:
        static SeqDesc * sel_with_atn_stop_desc = new SeqDesc[3]{
            {0, ScsiPhase::MESSAGE_OUT, SeqState::SEND_MSG_EX,  INTSTAT_SR | INTSTAT_SO},
            {1, -1,                     SeqState::CMD_COMPLETE, INTSTAT_SR | INTSTAT_SO},
        };
        this->seq_step = this->cur_step = 0;
        this->bytes_out = 1; // set message length
        this->cmd_steps = sel_with_atn_stop_desc;
        this->cur_state = SeqState::BUS_FREE;
        this->sequencer();
        LOG_F(9, "%s: SELECT WITH ATN AND STOP command started", this->name.c_str());
        break;
    case CMD_ENA_SEL_RESEL:
        exec_next_command();
        break;
    default:
        LOG_F(ERROR, "%s: invalid/unimplemented command 0x%X", this->name.c_str(), cmd);
        this->cmd_fifo_pos--; // remove invalid command from FIFO
        this->int_status = INTSTAT_ICMD;
        this->update_irq();
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

void Sc53C94::fifo_push(const uint8_t data)
{
    if (this->data_fifo_pos < DATA_FIFO_MAX) {
        this->data_fifo[this->data_fifo_pos++] = data;
    } else {
        LOG_F(ERROR, "%s: data FIFO overflow!", this->name.c_str());
        this->status |= STAT_GE; // signal IOE/Gross Error
    }
}

uint8_t Sc53C94::fifo_pop()
{
    uint8_t data = 0;

    if (this->data_fifo_pos < 1) {
        LOG_F(ERROR, "%s: data FIFO underflow!", this->name.c_str());
        this->status |= STAT_GE; // signal IOE/Gross Error
    } else {
        data = this->data_fifo[0];
        this->data_fifo_pos--;
        std::memmove(this->data_fifo, &this->data_fifo[1], this->data_fifo_pos);
    }

    return data;
}

void Sc53C94::seq_defer_state(uint64_t delay_ns)
{
    if (this->seq_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->seq_timer_id);
        this->seq_timer_id = 0;
    }

    if (delay_ns) {
        this->seq_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            delay_ns,
            [this]() {
                // re-enter the sequencer with the state specified in next_state
                this->seq_timer_id = 0;
                this->cur_state = this->next_state;
                this->sequencer();
        });
    } else {
        this->seq_timer_id = TimerManager::get_instance()->add_immediate_timer(
            [this]() {
                // re-enter the sequencer with the state specified in next_state
                this->seq_timer_id = 0;
                this->cur_state = this->next_state;
                this->sequencer();
        });
    }
}

void Sc53C94::sequencer()
{
    switch (this->cur_state) {
    case SeqState::IDLE:
        break;
    case SeqState::BUS_FREE:
        if (this->bus_obj->current_phase() == ScsiPhase::BUS_FREE) {
            this->next_state = SeqState::ARB_BEGIN;
            this->seq_defer_state(BUS_FREE_DELAY + BUS_SETTLE_DELAY);
        } else { // continue waiting
            this->next_state = SeqState::BUS_FREE;
            this->seq_defer_state(BUS_FREE_DELAY);
        }
        break;
    case SeqState::ARB_BEGIN:
        if (!this->bus_obj->begin_arbitration(this->my_bus_id)) {
            LOG_F(ERROR, "%s: arbitration error, bus not free!", this->name.c_str());
            this->bus_obj->release_ctrl_lines(this->my_bus_id);
            this->next_state = SeqState::BUS_FREE;
            this->seq_defer_state(BUS_CLEAR_DELAY);
            break;
        }
        this->next_state = SeqState::ARB_END;
        this->seq_defer_state(ARB_DELAY);
        break;
    case SeqState::ARB_END:
        if (this->bus_obj->end_arbitration(this->my_bus_id)) { // arbitration won
            this->next_state = SeqState::SEL_BEGIN;
            this->seq_defer_state(BUS_CLEAR_DELAY + BUS_SETTLE_DELAY);
        } else { // arbitration lost
            LOG_F(INFO, "%s: arbitration lost!", this->name.c_str());
            this->bus_obj->release_ctrl_lines(this->my_bus_id);
            this->next_state = SeqState::BUS_FREE;
            this->seq_defer_state(BUS_CLEAR_DELAY);
        }
        break;
    case SeqState::SEL_BEGIN:
        this->is_initiator = true;
        this->bus_obj->begin_selection(this->my_bus_id, this->target_id,
            this->cur_cmd != CMD_SELECT_NO_ATN);
        this->next_state = SeqState::SEL_END;
        this->seq_defer_state(SEL_TIME_OUT);
        break;
    case SeqState::SEL_END:
        if (this->bus_obj->end_selection(this->my_bus_id, this->target_id)) {
            this->bus_obj->release_ctrl_line(this->my_bus_id, SCSI_CTRL_SEL);
            LOG_F(9, "%s: selection completed", this->name.c_str());
        } else { // selection timeout
            this->seq_step = 0;
            this->int_status = INTSTAT_DIS;
            this->bus_obj->disconnect(this->my_bus_id);
            this->cur_state = SeqState::IDLE;
            this->update_irq();
            exec_next_command();
        }
        break;
    case SeqState::SEND_MSG:
    case SeqState::SEND_MSG_EX:
        if (this->data_fifo_pos < 1 && this->is_dma_cmd) {
            if (this->drq_cb)
                this->drq_cb(1);
        } else {
            this->bus_obj->target_xfer_data();
            if (this->cur_state == SeqState::SEND_MSG_EX) {
                this->notify(ScsiNotification::BUS_PHASE_CHANGE, ScsiPhase::MESSAGE_OUT);
            } else {
                this->bus_obj->release_ctrl_line(this->my_bus_id, SCSI_CTRL_ATN);
                if (this->cmd_steps)
                    this->bus_obj->target_next_step();
            }
        }
        break;
    case SeqState::SEND_CMD:
        if (this->data_fifo_pos < 1 && this->is_dma_cmd) {
            if (this->drq_cb)
                this->drq_cb(1);
        } else
            this->bus_obj->target_xfer_data();
        break;
    case SeqState::CMD_COMPLETE:
        this->cur_state = SeqState::IDLE;
        this->update_irq();
        exec_next_command();
        break;
    case SeqState::XFER_BEGIN:
        this->cur_bus_phase = this->bus_obj->current_phase();
        switch (this->cur_bus_phase) {
        case ScsiPhase::DATA_OUT:
            if (this->is_dma_cmd && this->channel_obj->is_ready()) {
                this->channel_obj->xfer_retry();
                break;
            }
            this->bus_obj->push_data(this->target_id, this->data_fifo, this->data_fifo_pos);
            this->data_fifo_pos = 0;
            this->cur_state = SeqState::XFER_END;
            this->sequencer();
            break;
        case ScsiPhase::DATA_IN:
            if (this->is_dma_cmd && this->channel_obj->is_ready()) {
                this->channel_obj->xfer_retry();
                break;
            }
            this->bus_obj->negotiate_xfer(this->data_fifo_pos, this->bytes_out);
            this->cur_state = SeqState::RCV_DATA;
            this->rcv_data();
            if (!(this->is_dma_cmd)) {
                this->cur_state = SeqState::XFER_END;
                this->sequencer();
            }
            break;
        case ScsiPhase::MESSAGE_IN:
        case ScsiPhase::MESSAGE_OUT:
            this->cur_state = (this->cur_bus_phase == ScsiPhase::MESSAGE_OUT) ?
                               SeqState::SEND_MSG : SeqState::RCV_MESSAGE;
            this->sequencer();
            this->cur_state = SeqState::XFER_END;
            this->sequencer();
            break;
        default:
            ABORT_F("%s: unsupported phase %d in XFER_BEGIN", this->name.c_str(),
                    this->cur_bus_phase);
        }
        break;
    case SeqState::XFER_END:
        if (this->is_initiator) {
            this->bus_obj->target_next_step();
        }
        this->int_status = INTSTAT_SR;
        this->cur_state = SeqState::IDLE;
        this->update_irq();
        exec_next_command();
        break;
    case SeqState::SEND_DATA:
        break;
    case SeqState::RCV_DATA:
        // check for unexpected bus phase changes
        if (this->bus_obj->current_phase() != this->cur_bus_phase) {
            this->cmd_fifo_pos = 0; // clear command FIFO
            this->int_status = INTSTAT_SR;
            this->update_irq();
        } else {
            this->rcv_data();
        }
        break;
    case SeqState::RCV_STATUS:
    case SeqState::RCV_MESSAGE:
        this->bus_obj->negotiate_xfer(this->data_fifo_pos, this->bytes_out);
        this->rcv_data();
        if (this->is_initiator) {
            if (this->cur_state == SeqState::RCV_STATUS) {
                this->bus_obj->target_next_step();
                if (this->cur_bus_phase == ScsiPhase::MESSAGE_IN) {
                    this->bus_obj->assert_ctrl_line(this->my_bus_id, SCSI_CTRL_REQ);
                    this->cur_state = SeqState::RCV_MESSAGE;
                    this->sequencer();
                }
            } else if (this->cur_state == SeqState::RCV_MESSAGE) {
                this->bus_obj->assert_ctrl_line(this->my_bus_id, SCSI_CTRL_ACK);
                if (this->cur_cmd == CMD_COMPLETE_STEPS) {
                    this->int_status = INTSTAT_SO;
                    this->cur_state = SeqState::CMD_COMPLETE;
                    this->sequencer();
                }
            }
        }
        break;
    default:
        ABORT_F("%s: unimplemented sequencer state %d", this->name.c_str(), this->cur_state);
    }
}

void Sc53C94::update_irq()
{
    uint8_t new_irq = !!(this->int_status != 0);
    if (new_irq != this->irq) {
        this->irq = new_irq;
        this->status = (this->status & ~STAT_INT) | (new_irq << 7);
        this->int_ctrl->ack_int(this->irq_id, new_irq);
    }
}

void Sc53C94::notify(ScsiNotification notif_type, int param)
{
    switch (notif_type) {
    case ScsiNotification::CONFIRM_SEL:
        if (this->target_id == param) {
            // cancel selection timeout timer
            TimerManager::get_instance()->cancel_timer(this->seq_timer_id);
            this->seq_timer_id = 0;
            this->cur_state = SeqState::SEL_END;
            this->sequencer();
        } else {
            LOG_F(WARNING, "%s: invalid selection confirmation message ignored",
                  this->name.c_str());
        }
        break;
    case ScsiNotification::BUS_PHASE_CHANGE:
        this->cur_bus_phase = param;
        if (param == ScsiPhase::BUS_FREE) { // target want to disconnect
            this->int_status = INTSTAT_DIS;
            this->update_irq();
            this->cur_state  = SeqState::IDLE;
        }
        if (this->cmd_steps != nullptr) {
            if (this->cur_bus_phase == this->cmd_steps->expected_phase) {
                this->next_state = this->cmd_steps->next_state;
                this->cmd_steps++;
                this->seq_defer_state(0);
            } else {
                this->cur_step   = this->cmd_steps->step_num;
                this->seq_step   = this->cur_step;
                this->int_status = this->cmd_steps->status;
                this->update_irq();
                if (this->cmd_steps->next_state == SeqState::CMD_COMPLETE)
                    this->exec_next_command();
            }
        }
        break;
    default:
        LOG_F(9, "%s: ignore notification message, type: %d", this->name.c_str(),
              notif_type);
    }
}

int Sc53C94::send_data(uint8_t* dst_ptr, int count)
{
    if (dst_ptr == nullptr || !count) {
        return 0;
    }

    int actual_count = std::min(this->data_fifo_pos, count);

    // move data out of the data FIFO
    std::memcpy(dst_ptr, this->data_fifo, actual_count);

    // remove the just readed data from the data FIFO
    this->data_fifo_pos -= actual_count;
    if (this->data_fifo_pos > 0) {
        std::memmove(this->data_fifo, &this->data_fifo[actual_count], this->data_fifo_pos);
    } else if (this->cur_bus_phase == ScsiPhase::DATA_OUT) {
        ABORT_F("%s: don't know what to do next!", this->name.c_str());
        this->sequencer();
    }

    return actual_count;
}

bool Sc53C94::rcv_data()
{
    int req_count;

    // return if REQ line is negated
    if (!this->bus_obj->test_ctrl_lines(SCSI_CTRL_REQ)) {
        return false;
    }

    if (this->is_dma_cmd && this->cur_bus_phase == ScsiPhase::DATA_IN) {
        req_count = std::min((int)this->xfer_count, DATA_FIFO_MAX - this->data_fifo_pos);
    } else {
        req_count = 1;
    }

    this->bus_obj->pull_data(this->target_id, &this->data_fifo[this->data_fifo_pos], req_count);
    this->data_fifo_pos += req_count;
    return true;
}

void Sc53C94::real_dma_xfer_out()
{
    // transfer data from host's memory to target

    if (this->xfer_count) {
        uint32_t got_bytes;
        uint8_t* src_ptr;
        this->dma_ch->pull_data(std::min((int)this->xfer_count, DATA_FIFO_MAX),
                                &got_bytes, &src_ptr);
        std::memcpy(this->data_fifo, src_ptr, got_bytes);
        this->data_fifo_pos = got_bytes;
        this->bus_obj->push_data(this->target_id, this->data_fifo, this->data_fifo_pos);

        this->xfer_count -= this->data_fifo_pos;
        this->data_fifo_pos = 0;
        if (!this->xfer_count) {
            this->status |= STAT_TC; // signal zero transfer count
            this->cur_state = SeqState::XFER_END;
            this->sequencer();
        }
    }

    if (this->xfer_count) {
        this->dma_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            10000,
            [this]() {
                // re-enter the sequencer with the state specified in next_state
                this->dma_timer_id = 0;
                this->real_dma_xfer_out();
        });
    }
}

void Sc53C94::real_dma_xfer_in()
{
    bool is_done = false;

    // transfer data from target to host's memory

    if (this->xfer_count && this->data_fifo_pos) {
        this->dma_ch->push_data((char*)this->data_fifo, this->data_fifo_pos);

        this->xfer_count -= this->data_fifo_pos;
        this->data_fifo_pos = 0;
        if (!this->xfer_count) {
            is_done = true;
            this->status |= STAT_TC; // signal zero transfer count
            this->cur_state = SeqState::XFER_END;
            this->sequencer();
        }
    }

    // see if we need to refill FIFO
    if (!this->data_fifo_pos && !is_done) {
        this->sequencer();
        this->dma_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            10000,
            [this]() {
                // re-enter the sequencer with the state specified in next_state
                this->dma_timer_id = 0;
                this->real_dma_xfer_in();
        });
    }
}

void Sc53C94::dma_wait() {
    if (this->cur_bus_phase == ScsiPhase::DATA_IN && this->cur_state == SeqState::RCV_DATA) {
        real_dma_xfer_in();
    }
    else if (this->cur_bus_phase == ScsiPhase::DATA_OUT && this->cur_state == SeqState::SEND_DATA) {
        real_dma_xfer_out();
    }
    else {
        this->dma_timer_id = TimerManager::get_instance()->add_oneshot_timer(
            10000,
            [this]() {
                this->dma_timer_id = 0;
                this->dma_wait();
        });
    }
}

void Sc53C94::dma_start()
{
    dma_wait();
}

void Sc53C94::dma_stop()
{
    if (this->dma_timer_id) {
        TimerManager::get_instance()->cancel_timer(this->dma_timer_id);
        this->dma_timer_id = 0;
    }
}

int Sc53C94::xfer_from(uint8_t *buf, int len) {
    int bytes_moved = 0;

    if (this->cur_cmd != CMD_XFER || !this->is_dma_cmd ||
        this->cur_bus_phase != ScsiPhase::DATA_IN) {
        LOG_F(9, "%s: ignoring DMA data transfer request", this->name.c_str());
        return bytes_moved;
    }

    len = std::min(len, (int)(this->xfer_count));

    // see if there are data bytes in the FIFO we want to grab first
    if (this->data_fifo_pos) {
        int fifo_bytes = std::min(this->data_fifo_pos, len);
        std::memcpy(buf, this->data_fifo, fifo_bytes);
        this->data_fifo_pos -= fifo_bytes;
        this->xfer_count -= fifo_bytes;
        len -= fifo_bytes;
        bytes_moved += fifo_bytes;
        buf += fifo_bytes;
        if (!this->xfer_count) {
            this->status |= STAT_TC; // signal zero transfer count
            this->cur_state = SeqState::XFER_END;
            this->sequencer();
            return bytes_moved;
        }
    }

    if (this->bus_obj->pull_data(this->target_id, buf, len)) {
        bytes_moved += len;
        this->xfer_count -= len;
        if (!this->xfer_count) {
            this->status |= STAT_TC; // signal zero transfer count
            this->cur_state = SeqState::XFER_END;
            this->sequencer();
        }
    }

    return bytes_moved;
}

int Sc53C94::xfer_to(uint8_t *buf, int len) {
    int bytes_moved = 0;

    if (!this->xfer_count || !this->is_dma_cmd) {
        LOG_F(9, "%s: ignoring DMA data transfer request", this->name.c_str());
        return bytes_moved;
    }

    len = std::min(len, (int)(this->xfer_count));

    // Being in the DATA_OUT phase means that we're about to move
    // a big chunk of data. The real device uses its FIFO as buffer.
    // For simplicity, the code below transfers the whole chunk at once.
    // This can be broken into smaller chunks later if desired.
    if (this->cur_bus_phase == ScsiPhase::DATA_OUT) {
        if (this->bus_obj->push_data(this->target_id, buf, len)) {
            this->xfer_count -= len;
            bytes_moved += len;
            if (!this->xfer_count) {
                this->status |= STAT_TC; // signal zero transfer count
                this->cur_state = SeqState::XFER_END;
                this->sequencer();
            }
            len = 0;
        } else
            LOG_F(WARNING, "%s: xfer_to failed to transfer data", this->name.c_str());
    }

    if (this->xfer_count) {
        // fill in the data FIFO first
        uint32_t fifo_bytes = std::min(len, DATA_FIFO_MAX - this->data_fifo_pos);
        std::memcpy(&this->data_fifo[this->data_fifo_pos], buf, fifo_bytes);
        this->data_fifo_pos += fifo_bytes;
        this->xfer_count -= fifo_bytes;
        bytes_moved += fifo_bytes;
        if (!this->xfer_count) {
            this->status |= STAT_TC; // signal zero transfer count
            this->sequencer();
        }
    }

    return bytes_moved;
}

static const PropMap Sc53C94_properties = {
    {"hdd_img", new StrProperty("")},
    {"cdr_img", new StrProperty("")},
};

static const std::vector<std::string> Sc53C94_Subdevices = {
    "ScsiCurio"
};

static const DeviceDescription Sc53C94_Descriptor = {
    Sc53C94::create, Sc53C94_Subdevices, Sc53C94_properties, HWCompType::SCSI_HOST | HWCompType::SCSI_DEV
};

REGISTER_DEVICE(Sc53C94, Sc53C94_Descriptor);
