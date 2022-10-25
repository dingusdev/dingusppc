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

/** @file SCSI bus emulation. */

#include <devices/common/hwcomponent.h>
#include <devices/common/scsi/scsi.h>
#include <loguru.hpp>

#include <cinttypes>

ScsiBus::ScsiBus()
{
    supports_types(HWCompType::SCSI_BUS);

    for(int i = 0; i < SCSI_MAX_DEVS; i++) {
        this->devices[i]        = nullptr;
        this->dev_ctrl_lines[i] = 0;
    }

    this->ctrl_lines = 0; // all control lines released
    this->data_lines = 0; // data lines released
    this->arb_winner_id = -1;
    this->initiator_id = -1;
    this->target_id = -1;
}

void ScsiBus::register_device(int id, ScsiDevice* dev_obj)
{
    if (this->devices[id] != nullptr) {
        ABORT_F("ScsiBus: device with ID %d already registered", id);
    }

    this->devices[id] = dev_obj;
}

void ScsiBus::change_bus_phase(int initiator_id)
{
    for (int i = 0; i < SCSI_MAX_DEVS; i++) {
        if (i == initiator_id)
            continue; // don't notify the initiator
        if (this->devices[i] != nullptr) {
            this->devices[i]->notify(this, ScsiMsg::BUS_PHASE_CHANGE, this->cur_phase);
        }
    }
}

void ScsiBus::assert_ctrl_line(int initiator_id, uint16_t mask)
{
    uint16_t new_state = 0xFFFFU & mask;

    this->dev_ctrl_lines[initiator_id] = new_state;

    if (new_state == this->ctrl_lines) {
        return;
    }

    this->ctrl_lines = new_state;

    if (new_state & SCSI_CTRL_RST) {
        this->cur_phase = ScsiPhase::RESET;
        change_bus_phase(initiator_id);
    }
}

void ScsiBus::release_ctrl_line(int id, uint16_t mask)
{
    uint16_t new_state = 0;

    this->dev_ctrl_lines[id] &= ~mask;

    // OR control lines of all devices together
    for (int i = 0; i < SCSI_MAX_DEVS; i++) {
        new_state |= this->dev_ctrl_lines[i];
    }

    if (this->ctrl_lines & SCSI_CTRL_RST) {
        if (!(new_state & SCSI_CTRL_RST)) {
            this->ctrl_lines = new_state;
            this->cur_phase = ScsiPhase::BUS_FREE;
            change_bus_phase(id);
        }
    }
}

void ScsiBus::release_ctrl_lines(int id)
{
    this->release_ctrl_line(id, 0xFFFFUL);
}

bool ScsiBus::begin_arbitration(int initiator_id)
{
    if (this->cur_phase == ScsiPhase::BUS_FREE) {
        this->data_lines |= 1 << initiator_id;
        this->cur_phase = ScsiPhase::ARBITRATION;
        change_bus_phase(initiator_id);
        return true;
    } else {
        return false;
    }
}

bool ScsiBus::end_arbitration(int initiator_id)
{
    int highest_id = -1;

    // find the highest ID bit on the data lines
    for (int id = 7; id >= 0; id--) {
        if (this->data_lines & (1 << id)) {
            highest_id = id;
            break;
        }
    }

    if (highest_id >= 0) {
        this->arb_winner_id = highest_id;
    }

    return highest_id == initiator_id;
}

bool ScsiBus::begin_selection(int initiator_id, int target_id, bool atn)
{
    // perform bus integrity checks
    if (this->cur_phase != ScsiPhase::ARBITRATION || this->arb_winner_id != initiator_id)
        return false;

    this->data_lines |= (1 << initiator_id) | (1 << target_id);

    if (atn) {
        assert_ctrl_line(initiator_id, SCSI_CTRL_ATN);
    }

    this->initiator_id = initiator_id;
    this->cur_phase = ScsiPhase::SELECTION;
    change_bus_phase(initiator_id);
    return true;
}

void ScsiBus::confirm_selection(int target_id)
{
    this->target_id = target_id;

    // notify initiator about selection confirmation from target
    if (this->initiator_id >= 0) {
        this->devices[this->initiator_id]->notify(this, ScsiMsg::CONFIRM_SEL, target_id);
    }
}

bool ScsiBus::end_selection(int initiator_id, int target_id)
{
    // check for selection confirmation from target
    return this->target_id == target_id;
}

void ScsiBus::disconnect(int dev_id)
{
    this->release_ctrl_lines(dev_id);
    if (!(this->ctrl_lines & SCSI_CTRL_BSY) && !(this->ctrl_lines & SCSI_CTRL_SEL)) {
        this->cur_phase = ScsiPhase::BUS_FREE;
        change_bus_phase(dev_id);
    }
}
