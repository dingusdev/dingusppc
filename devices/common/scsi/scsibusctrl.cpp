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

/** @file SCSI bus controller emulation. */

#include <core/timermanager.h>
#include <devices/common/scsi/scsibusctrl.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cstring>

using namespace Scsi_Bus_Controller;

void ScsiBusController::seq_defer_state(uint64_t delay_ns) {
    seq_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        delay_ns,
        [this]() {
            // re-enter the sequencer with the state specified in next_state
            this->cur_state = this->next_state;
            this->sequencer();
    });
}

void ScsiBusController::sequencer() {
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
        if (!this->bus_obj->begin_arbitration(this->src_id)) {
            LOG_F(ERROR, "%s: arbitration error, bus not free!", this->name.c_str());
            this->bus_obj->release_ctrl_lines(this->src_id);
            this->next_state = SeqState::BUS_FREE;
            this->seq_defer_state(BUS_CLEAR_DELAY);
            break;
        }
        this->next_state = SeqState::ARB_END;
        this->seq_defer_state(ARB_DELAY);
        break;
    case SeqState::ARB_END:
        if (this->bus_obj->end_arbitration(this->src_id) &&
            !this->bus_obj->test_ctrl_lines(SCSI_CTRL_SEL)) { // arbitration won
            this->bus_obj->assert_ctrl_line(this->src_id, SCSI_CTRL_SEL);
            this->step_completed();
        } else { // arbitration lost
            LOG_F(ERROR, "%s: arbitration lost!", this->name.c_str());
            this->bus_obj->release_ctrl_lines(this->src_id);
            this->report_error(ARB_LOST);
        }
        break;
    case SeqState::SEL_BEGIN:
        this->bus_obj->begin_selection(this->src_id, this->dst_id, this->assert_atn);
        this->next_state = SeqState::SEL_END;
        this->seq_defer_state(SEL_TIME_OUT);
        break;
    case SeqState::SEL_END:
        if (this->bus_obj->end_selection(this->src_id, this->dst_id)) {
            this->bus_obj->release_ctrl_line(this->src_id, SCSI_CTRL_SEL);
            LOG_F(9, "%s: selection completed", this->name.c_str());
            this->step_completed();
        } else { // selection timeout
            this->bus_obj->disconnect(this->src_id);
            this->cur_state = SeqState::IDLE;
            this->report_error(SEL_TIMEOUT);
        }
        break;
    case SeqState::SEND_MSG:
        if (this->fifo_pos) {
            this->bus_obj->target_xfer_data();
            this->bus_obj->release_ctrl_line(this->src_id, SCSI_CTRL_ATN);
            if (this->to_xfer <= 0)
                this->step_completed();
        }
        break;
    case SeqState::SEND_CMD:
        this->bus_obj->target_xfer_data();
        if (!this->fifo_pos)
            this->step_completed();
        break;
    case SeqState::XFER_BEGIN:
        this->cur_bus_phase = this->bus_obj->current_phase();
        switch (this->cur_bus_phase) {
        case ScsiPhase::DATA_OUT:
            this->cur_state = SeqState::SEND_DATA;
            break;
        case ScsiPhase::DATA_IN:
            this->bus_obj->negotiate_xfer(this->fifo_pos, this->bytes_out);
            this->cur_state = SeqState::RCV_DATA;
            this->rcv_data();
        }
        break;
    case SeqState::XFER_END:
        if (this->is_initiator)
            this->bus_obj->target_next_step();
        this->step_completed();
        break;
    case SeqState::SEND_DATA:
        if (this->bus_obj->push_data(this->dst_id, this->data_fifo, this->fifo_pos)) {
            this->to_xfer -= this->fifo_pos;
            this->fifo_pos = 0;
            if (this->to_xfer <= 0) {
                this->cur_state = SeqState::XFER_END;
                this->sequencer();
            }
        }
        break;
    case SeqState::RCV_DATA:
        // check for unexpected bus phase changes
        if (this->bus_obj->current_phase() != this->cur_bus_phase) {
            LOG_F(WARNING, "%s: phase mismatch!", this->name.c_str());
        } else {
            if (!this->rcv_data()) {
                this->cur_state = SeqState::XFER_END;
                this->sequencer();
            }
        }
        break;
    case SeqState::RCV_STATUS:
    case SeqState::RCV_MESSAGE:
        this->bus_obj->negotiate_xfer(this->fifo_pos, this->bytes_out);
        this->rcv_data();
        if (this->is_initiator) {
            if (this->cur_state == SeqState::RCV_MESSAGE)
                this->bus_obj->assert_ctrl_line(this->src_id, SCSI_CTRL_ACK);
            this->bus_obj->target_next_step();
            this->step_completed();
            this->cur_state = SeqState::IDLE;
        }
        break;
    default:
        ABORT_F("%s: unimplemented sequencer state %d", this->name.c_str(),
                this->cur_state);
    }
}

void ScsiBusController::notify(ScsiMsg msg_type, int param) {
    switch (msg_type) {
    case ScsiMsg::CONFIRM_SEL:
        if (this->dst_id == param) {
            // cancel selection timeout timer
            TimerManager::get_instance()->cancel_timer(this->seq_timer_id);
            seq_timer_id = 0;
            this->cur_state = SeqState::SEL_END;
            this->sequencer();
        } else {
            LOG_F(WARNING, "%s: ignore invalid selection confirmation message",
                  this->name.c_str());
        }
        break;
    case ScsiMsg::BUS_PHASE_CHANGE:
        this->cur_bus_phase = param;
#if 0
        if (param != ScsiPhase::BUS_FREE && this->cmd_steps != nullptr) {
            this->cmd_steps++;
            this->cur_state = this->cmd_steps->next_step;
            this->sequencer();
        }
#endif
        break;
    default:
        LOG_F(9, "%s: ignore notification message, type: %d", this->name.c_str(),
              msg_type);
    }
}

bool ScsiBusController::rcv_data() {
    int req_count;

    // return if REQ line is negated
    if (!this->bus_obj->test_ctrl_lines(SCSI_CTRL_REQ)) {
        return false;
    }

    if (!this->to_xfer)
        return false;

    req_count = std::min(this->to_xfer, DATA_FIFO_DEPTH - this->fifo_pos);

    this->bus_obj->pull_data(this->dst_id, &this->data_fifo[this->fifo_pos], req_count);
    this->fifo_pos += req_count;
    this->to_xfer  -= req_count;
    return true;
}

int ScsiBusController::send_data(uint8_t* dst_ptr, int count) {
    if (dst_ptr == nullptr || !count)
        return 0;

    int actual_count = std::min(this->fifo_pos, count);

    // move data out of the data FIFO
    std::memcpy(dst_ptr, this->data_fifo, actual_count);

    // remove the just readed data from the data FIFO
    this->fifo_pos -= actual_count;
    this->to_xfer  -= actual_count;
    if (this->fifo_pos > 0)
        std::memmove(this->data_fifo, &this->data_fifo[actual_count], this->fifo_pos);

    return actual_count;
}

int ScsiBusController::xfer_from(uint8_t *buf, int len) {
    if (len > this->to_xfer + this->fifo_pos)
        LOG_F(WARNING, "%s: DMA xfer len > command xfer len", this->name.c_str());

    if (this->fifo_pos) {
        int fifo_bytes = std::min(this->fifo_pos, len);
        std::memcpy(buf, this->data_fifo, fifo_bytes);
        this->fifo_pos -= fifo_bytes;
        len -= fifo_bytes;
        buf += fifo_bytes;
    }

    int dma_bytes = std::min(this->to_xfer, len);

    if (this->bus_obj->pull_data(this->dst_id, buf, dma_bytes)) {
        this->to_xfer -= dma_bytes;
        if (this->to_xfer <= 0) {
            this->xfer_count = this->to_xfer;
            this->cur_state = SeqState::XFER_END;
            this->sequencer();
        }
        return 0;
    }

    return len;
}

void ScsiBusController::update_irq() {
    uint8_t new_irq = !!(this->int_stat & this->int_mask);
    if (new_irq != this->irq) {
        this->irq = new_irq;
        this->int_ctrl->ack_int(this->irq_id, new_irq);
    }
}

void ScsiBusController::fifo_push(const uint8_t data) {
    if (this->fifo_pos < DATA_FIFO_DEPTH) {
        this->data_fifo[this->fifo_pos++] = data;
        if (!this->xfer_count)
            LOG_F(WARNING, "%s: zero xfer_count!", this->name.c_str());
        if (--this->xfer_count == 0)
            this->sequencer();
    } else
        this->sequencer();
}

uint8_t ScsiBusController::fifo_pop() {
    uint8_t data = 0;

    if (this->fifo_pos) {
        data = this->data_fifo[0];
        if (--this->fifo_pos)
            std::memmove(this->data_fifo, &this->data_fifo[1], this->fifo_pos);
    }

    // see if we need to refill FIFO
    if (!this->fifo_pos)
        this->sequencer();

    return data;
}
