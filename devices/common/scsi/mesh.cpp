/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

#include <devices/common/scsi/mesh.h>
#include <devices/deviceregistry.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

#include <cinttypes>

using namespace MeshScsi;

int MeshController::device_postinit()
{
    this->bus_obj = dynamic_cast<ScsiBus*>(gMachineObj->get_comp_by_name("SCSI0"));

    return 0;
}

void MeshController::reset(bool is_hard_reset)
{
    this->cur_cmd       = SeqCmd::NoOperation;
    this->int_mask      = 0;

    if (is_hard_reset) {
        this->bus_stat      = 0;
        this->sync_params   = 2; // guessed
    }
}

uint8_t MeshController::read(uint8_t reg_offset)
{
    switch(reg_offset) {
    case MeshReg::BusStatus0:
        return this->bus_obj->test_ctrl_lines(0xFFU);
    case MeshReg::BusStatus1:
        return this->bus_obj->test_ctrl_lines(0xE000U) >> 8;
    case MeshReg::IntMask:
        return this->int_mask;
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
                        this->bus_obj->assert_ctrl_line(new_stat, mask);
                    else
                        this->bus_obj->release_ctrl_line(new_stat, mask);
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
    default:
        LOG_F(WARNING, "MESH: write to unimplemented register at offset 0x%x",
              reg_offset);
    }
}

void MeshController::perform_command(const uint8_t cmd)
{
    this->cur_cmd = cmd & 0xF;

    this->int_stat &= ~INT_CMD_DONE;

    switch (this->cur_cmd) {
    case SeqCmd::ResetMesh:
        this->reset(false);
        this->int_stat |= INT_CMD_DONE;
        break;
    default:
        LOG_F(ERROR, "MESH: unsupported sequencer command 0x%X", this->cur_cmd);
    }
}

static const DeviceDescription Mesh_Descriptor = {
    MeshController::create, {}, {}
};

REGISTER_DEVICE(Mesh, Mesh_Descriptor);
