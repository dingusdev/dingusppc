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

/** @file MESH (Macintosh Enhanced SCSI Hardware) controller emulation. */

#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/hwinterrupt.h>
#include <devices/common/scsi/mesh.h>
#include <devices/common/scsi/scsi.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>

using namespace MeshScsi;

int MeshController::device_postinit() {
    this->bus_obj = dynamic_cast<ScsiBus*>(gMachineObj->get_comp_by_name("ScsiMesh"));
    if (bus_obj) {
        bus_obj->register_device(7, static_cast<ScsiPhysDevice*>(this));
        bus_obj->attach_scsi_devices("2");
    }

    this->int_ctrl = dynamic_cast<InterruptCtrl*>(
        gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
    this->irq_id = this->int_ctrl->register_dev_int(IntSrc::SCSI_MESH);

    return 0;
}

void MeshController::reset(bool is_hard_reset) {
    this->cur_cmd       = SeqCmd::NoOperation;
    this->fifo_pos      = 0;
    this->int_mask      = 0;
    this->exception     = 0;
    this->xfer_count    = 0;
    this->src_id        = 7;

    if (is_hard_reset) {
        this->bus_stat    = 0;
        this->sync_params = (0 << 16) | 2; // fast async operation (guessed)
    }
}

uint8_t MeshController::read(uint8_t reg_offset) {
    switch(reg_offset) {
    case MeshReg::XferCount0:
        return this->xfer_count & 0xFFU;
    case MeshReg::XferCount1:
        return (this->xfer_count >> 8) & 0xFFU;
    case MeshReg::FIFO:
        return this->fifo_pop();
    case MeshReg::Sequence:
        return this->cur_cmd;
    case MeshReg::BusStatus0:
        return this->bus_obj->test_ctrl_lines(0xFFU);
    case MeshReg::BusStatus1:
        return this->bus_obj->test_ctrl_lines(0xE000U) >> 8;
    case MeshReg::FIFOCount:
        return this->fifo_pos;
    case MeshReg::Exception:
        return this->exception;
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

void MeshController::write(uint8_t reg_offset, uint8_t value) {
    //uint16_t new_stat;

    switch(reg_offset) {
    case MeshReg::XferCount0:
        this->xfer_count = (this->xfer_count & ~0xFFU) | value;
        break;
    case MeshReg::XferCount1:
        this->xfer_count = (this->xfer_count & ~0xFF00U) | (value << 8);
        break;
    case MeshReg::FIFO:
        this->fifo_push(value);
        break;
    case MeshReg::Sequence:
        perform_command(value);
        break;
    case MeshReg::BusStatus0:
        this->update_bus_status((this->bus_stat & 0xFF00U) | value);
        break;
    case MeshReg::BusStatus1:
        this->update_bus_status((this->bus_stat & 0xFFU) | (value << 8));
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

void MeshController::perform_command(const uint8_t cmd) {
    this->cur_cmd = cmd;

    this->int_stat &= ~INT_CMD_DONE;

    this->is_dma_cmd = !!(this->cur_cmd & 0x80);

    switch (this->cur_cmd & 0xF) {
    case SeqCmd::Arbitrate:
        this->exception &= EXC_ARB_LOST;
        this->bus_obj->release_ctrl_lines(this->src_id);
        this->cur_state = Scsi_Bus_Controller::SeqState::BUS_FREE;
        this->sequencer();
        break;
    case SeqCmd::Select:
        this->assert_atn = !!(this->cur_cmd & 0x20);
        this->exception &= EXC_SEL_TIMEOUT;
        this->cur_state = Scsi_Bus_Controller::SeqState::SEL_BEGIN;
        this->sequencer();
        break;
    case SeqCmd::Command:
        if (this->bus_obj->current_phase() != ScsiPhase::COMMAND)
            LOG_F(WARNING, "%s: not in COMMAND phase", this->name.c_str());
        this->cur_state = Scsi_Bus_Controller::SeqState::SEND_CMD;
        if (this->fifo_pos)
            this->sequencer();
        break;
    case SeqCmd::Status:
        if (this->bus_obj->current_phase() != ScsiPhase::STATUS)
            LOG_F(WARNING, "%s: not in STATUS phase", this->name.c_str());
        this->to_xfer = this->xfer_count;
        this->cur_state = Scsi_Bus_Controller::SeqState::RCV_STATUS;
        this->sequencer();
        break;
    case SeqCmd::DataOut:
        if (this->bus_obj->current_phase() != ScsiPhase::DATA_OUT)
            LOG_F(WARNING, "%s: not in DATA OUT phase", this->name.c_str());
        this->to_xfer = this->xfer_count ? this->xfer_count : 65536;
        this->cur_state = Scsi_Bus_Controller::SeqState::XFER_BEGIN;
        this->sequencer();
        break;
    case SeqCmd::DataIn:
        if (this->bus_obj->current_phase() != ScsiPhase::DATA_IN)
            LOG_F(WARNING, "%s: not in DATA IN phase", this->name.c_str());
        this->to_xfer = this->xfer_count ? this->xfer_count : 65536;
        this->cur_state = Scsi_Bus_Controller::SeqState::XFER_BEGIN;
        this->sequencer();
        break;
    case SeqCmd::MessageOut:
        if (this->bus_obj->current_phase() != ScsiPhase::MESSAGE_OUT)
            LOG_F(WARNING, "%s: not in MESSAGE OUT phase", this->name.c_str());
        this->to_xfer = this->xfer_count;
        this->cur_state = Scsi_Bus_Controller::SeqState::SEND_MSG;
        this->sequencer();
        break;
    case SeqCmd::MessageIn:
        if (this->bus_obj->current_phase() != ScsiPhase::MESSAGE_IN)
            LOG_F(WARNING, "%s: not in MESSAGE IN phase", this->name.c_str());
        this->to_xfer = this->xfer_count;
        this->cur_state = Scsi_Bus_Controller::SeqState::RCV_MESSAGE;
        this->sequencer();
        break;
    case SeqCmd::BusFree:
        // Don't release ACK if ATN is asserted. This condition indicates
        // that the initiator wants to reject last message.
        if (!this->bus_obj->test_ctrl_lines(SCSI_CTRL_ATN))
            this->bus_obj->release_ctrl_line(this->src_id, SCSI_CTRL_ACK);
        // Let the target switch to next bus phase if
        // - both ACK and ATN are asserted so the target will reject last message
        // - when in MESSAGE_IN phase to tell the target the initiator accepts last message
        if (this->is_initiator && (this->bus_obj->test_ctrl_lines(SCSI_CTRL_ATN) ||
            this->bus_obj->current_phase() == ScsiPhase::MESSAGE_IN))
            this->bus_obj->target_next_step();
        // generate phase mismatch exception
        // if the target is still connected
        if (this->bus_obj->test_ctrl_lines(SCSI_CTRL_BSY)) {
            this->exception |= EXC_PHASE_MM;
            this->int_stat  |= INT_EXCEPTION;
        } else // say ok because we got the expected bus free phase
            this->int_stat |= INT_CMD_DONE;
        this->update_irq();
        break;
    case SeqCmd::EnaParityCheck:
        this->check_parity = true;
        break;
    case SeqCmd::DisParityCheck:
        this->check_parity = false;
        break;
    case SeqCmd::EnaReselect:
        LOG_F(9, "MESH: EnaReselect stub invoked");
        this->int_stat |= INT_CMD_DONE;
        break;
    case SeqCmd::DisReselect:
        LOG_F(9, "MESH: DisReselect stub invoked");
        this->int_stat |= INT_CMD_DONE;
        break;
    case SeqCmd::ResetMesh:
        this->reset(false);
        this->int_stat |= INT_CMD_DONE;
        update_irq();
        break;
    case SeqCmd::FlushFIFO:
        this->fifo_pos = 0;
        break;
    default:
        LOG_F(ERROR, "MESH: unsupported sequencer command 0x%X", this->cur_cmd);
    }
}

void MeshController::update_bus_status(const uint16_t new_stat) {
    uint16_t mask;

    // update the lower part (BusStatus0)
    if ((new_stat ^ this->bus_stat) & 0xFF) {
        for (mask = SCSI_CTRL_REQ; mask >= SCSI_CTRL_IO; mask >>= 1) {
            if ((new_stat ^ this->bus_stat) & mask) {
                if (new_stat & mask)
                    this->bus_obj->assert_ctrl_line(this->src_id, mask);
                else
                    this->bus_obj->release_ctrl_line(this->src_id, mask);
            }
        }
    }

    // update the upper part (BusStatus1)
    if ((new_stat ^ this->bus_stat) & 0xFF00U) {
        for (mask = SCSI_CTRL_RST; mask >= SCSI_CTRL_SEL; mask >>= 1) {
            if ((new_stat ^ this->bus_stat) & mask) {
                if (new_stat & mask)
                    this->bus_obj->assert_ctrl_line(this->src_id, mask);
                else
                    this->bus_obj->release_ctrl_line(this->src_id, mask);
            }
        }
    }

    this->bus_stat = new_stat;
}

void MeshController::step_completed() {
    this->int_stat |= INT_CMD_DONE;
    update_irq();
}

void MeshController::report_error(const int error) {
    switch (error) {
    case ARB_LOST:
        this->exception |= EXC_ARB_LOST;
        this->int_stat |= INT_EXCEPTION;
        break;
    case SEL_TIMEOUT:
        this->exception |= EXC_SEL_TIMEOUT;
        this->int_stat  |= INT_EXCEPTION;
        break;
    default:
        ABORT_F("%s: unhandled error %d", this->name.c_str(), error);
    }
    this->int_stat |= INT_CMD_DONE;
    update_irq();
}

static const PropMap Mesh_properties = {
    {"hdd_img2", new StrProperty("")},
    {"cdr_img2", new StrProperty("")},
};

static const std::vector<std::string> Mesh_Subdevices = {
    "ScsiMesh"
};

static const DeviceDescription Mesh_Tnt_Descriptor = {
    MeshController::create_for_tnt, Mesh_Subdevices, Mesh_properties, HWCompType::SCSI_HOST | HWCompType::SCSI_DEV
};

static const DeviceDescription Mesh_Heathrow_Descriptor = {
    MeshController::create_for_heathrow, Mesh_Subdevices, Mesh_properties, HWCompType::SCSI_HOST | HWCompType::SCSI_DEV
};

REGISTER_DEVICE(MeshTnt,      Mesh_Tnt_Descriptor);
REGISTER_DEVICE(MeshHeathrow, Mesh_Heathrow_Descriptor);
