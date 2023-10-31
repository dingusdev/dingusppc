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

/** @file MESH (Macintosh Enhanced SCSI Hardware) controller emulation. */

#include <core/timermanager.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/scsi/mesh.h>
#include <devices/common/scsi/scsi.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>

using namespace MeshScsi;

int MeshController::device_postinit()
{
    this->bus_obj = dynamic_cast<ScsiBus*>(gMachineObj->get_comp_by_name("ScsiMesh"));

    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::SCSI_MESH);

    return 0;
}

void MeshController::reset(bool is_hard_reset)
{
    this->cur_cmd       = SeqCmd::NoOperation;
    this->fifo_cnt      = 0;
    this->int_mask      = 0;
    this->xfer_count    = 0;
    this->src_id        = 7;

    if (is_hard_reset) {
        this->bus_stat      = 0;
        this->sync_params   = (0 << 16) | 2; // fast async operation (guessed)
    }
}

uint8_t MeshController::read(uint8_t reg_offset)
{
    switch(reg_offset) {
    case MeshReg::XferCount0:
        return this->xfer_count & 0xFFU;
    case MeshReg::XferCount1:
        return (this->xfer_count >> 8) & 0xFFU;
    case MeshReg::Sequence:
        return this->cur_cmd;
    case MeshReg::BusStatus0:
        return this->bus_obj->test_ctrl_lines(0xFFU);
    case MeshReg::BusStatus1:
        return this->bus_obj->test_ctrl_lines(0xE000U) >> 8;
    case MeshReg::FIFOCount:
        return this->fifo_cnt;
    case MeshReg::Exception:
        return 0;
    case MeshReg::Error:
        return 0;
    case MeshReg::IntMask:
        return this->int_mask;
    case MeshReg::Interrupt:
        return this->int_stat;
    case MeshReg::DestID:
        return this->dst_id;
    case MeshReg::SyncParms:
        return this->sync_params;
    case MeshReg::MeshID:
        return this->chip_id; // tell them who we are
    default:
        LOG_F(WARNING, "MESH: read from unimplemented register at offset 0x%x", reg_offset);
    }

    return 0;
}

void MeshController::write(uint8_t reg_offset, uint8_t value)
{
    uint16_t new_stat;

    switch(reg_offset) {
    case MeshReg::Sequence:
        perform_command(value);
        break;
    case MeshReg::BusStatus1:
        new_stat = value << 8;
        if (new_stat != this->bus_stat) {
            for (uint16_t mask = SCSI_CTRL_RST; mask >= SCSI_CTRL_SEL; mask >>= 1) {
                if ((new_stat ^ this->bus_stat) & mask) {
                    if (new_stat & mask)
                        this->bus_obj->assert_ctrl_line(this->src_id, mask);
                    else
                        this->bus_obj->release_ctrl_line(this->src_id, mask);
                }
            }
            this->bus_stat = new_stat;
        }
        break;
    case MeshReg::IntMask:
        this->int_mask = value;
        break;
    case MeshReg::Interrupt:
        this->int_stat &= ~(value & INT_MASK); // clear requested interrupt bits
        update_irq();
        break;
    case MeshReg::SourceID:
        this->src_id = value;
        break;
    case MeshReg::DestID:
        this->dst_id = value;
        break;
    case MeshReg::SyncParms:
        this->sync_params = value;
        break;
    case MeshReg::SelTimeOut:
        LOG_F(9, "MESH: selection timeout set to 0x%x", value);
        break;
    default:
        LOG_F(WARNING, "MESH: write to unimplemented register at offset 0x%x",
              reg_offset);
    }
}

void MeshController::perform_command(const uint8_t cmd)
{
    this->cur_cmd = cmd;

    this->int_stat &= ~INT_CMD_DONE;

    switch (this->cur_cmd & 0xF) {
    case SeqCmd::Arbitrate:
        this->bus_obj->release_ctrl_lines(this->src_id);
        this->cur_state = SeqState::BUS_FREE;
        this->sequencer();
        break;
    case SeqCmd::Select:
        this->cur_state = SeqState::SEL_BEGIN;
        this->sequencer();
        break;
    case SeqCmd::BusFree:
        LOG_F(INFO, "MESH: BusFree stub invoked");
        this->int_stat |= INT_CMD_DONE;
        break;
    case SeqCmd::EnaReselect:
        LOG_F(INFO, "MESH: EnaReselect stub invoked");
        this->int_stat |= INT_CMD_DONE;
        break;
    case SeqCmd::DisReselect:
        LOG_F(9, "MESH: DisReselect stub invoked");
        this->int_stat |= INT_CMD_DONE;
        break;
    case SeqCmd::ResetMesh:
        this->reset(false);
        this->int_stat |= INT_CMD_DONE;
        break;
    case SeqCmd::FlushFIFO:
        LOG_F(INFO, "MESH: FlushFIFO stub invoked");
        this->int_stat |= INT_CMD_DONE;
        break;
    default:
        LOG_F(ERROR, "MESH: unsupported sequencer command 0x%X", this->cur_cmd);
    }
}

void MeshController::seq_defer_state(uint64_t delay_ns)
{
    seq_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        delay_ns,
        [this]() {
            // re-enter the sequencer with the state specified in next_state
            this->cur_state = this->next_state;
            this->sequencer();
    });
}

void MeshController::sequencer()
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
        if (!this->bus_obj->begin_arbitration(this->src_id)) {
            LOG_F(ERROR, "MESH: arbitration error, bus not free!");
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
        } else { // arbitration lost
            LOG_F(INFO, "MESH: arbitration lost!");
            this->bus_obj->release_ctrl_lines(this->src_id);
            this->exception |= EXC_ARB_LOST;
            this->int_stat  |= INT_EXCEPTION;
        }
        this->int_stat |= INT_CMD_DONE;
        update_irq();
        break;
    case SeqState::SEL_BEGIN:
        this->bus_obj->begin_selection(this->src_id, this->dst_id, this->cur_cmd & 0x20);
        this->next_state = SeqState::SEL_END;
        this->seq_defer_state(SEL_TIME_OUT);
        break;
    case SeqState::SEL_END:
        if (this->bus_obj->end_selection(this->src_id, this->dst_id)) {
            this->bus_obj->release_ctrl_line(this->src_id, SCSI_CTRL_SEL);
            LOG_F(9, "MESH: selection completed");
        } else { // selection timeout
            this->bus_obj->disconnect(this->src_id);
            this->cur_state = SeqState::IDLE;
            this->exception |= EXC_SEL_TIMEOUT;
            this->int_stat  |= INT_EXCEPTION;
        }
        this->int_stat |= INT_CMD_DONE;
        update_irq();
        break;
    default:
        ABORT_F("MESH: unimplemented sequencer state %d", this->cur_state);
    }
}

void MeshController::update_irq()
{
    uint8_t new_irq = !!(this->int_stat & this->int_mask);
    if (new_irq != this->irq) {
        this->irq = new_irq;
        this->int_ctrl->ack_int(this->irq_id, new_irq);
    }
}

static const PropMap Mesh_properties = {
    {"hdd_img2", new StrProperty("")},
    {"cdr_img2", new StrProperty("")},
};

static const DeviceDescription Mesh_Tnt_Descriptor = {
    MeshController::create_for_tnt, {}, Mesh_properties
};

static const DeviceDescription Mesh_Heathrow_Descriptor = {
    MeshController::create_for_heathrow, {}, Mesh_properties
};

REGISTER_DEVICE(MeshTnt,      Mesh_Tnt_Descriptor);
REGISTER_DEVICE(MeshHeathrow, Mesh_Heathrow_Descriptor);
