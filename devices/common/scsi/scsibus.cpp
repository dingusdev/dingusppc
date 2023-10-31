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

/** @file SCSI bus emulation. */

#include <devices/common/hwcomponent.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/common/scsi/scsicdrom.h>
#include <devices/deviceregistry.h>
#include <machines/machinebase.h>
#include <loguru.hpp>

#include <cinttypes>
#include <sstream>

ScsiBus::ScsiBus(const std::string name)
{
    this->set_name(name);
    supports_types(HWCompType::SCSI_BUS);

    for(int i = 0; i < SCSI_MAX_DEVS; i++) {
        this->devices[i]        = nullptr;
        this->dev_ctrl_lines[i] = 0;
    }

    this->ctrl_lines    =  0; // all control lines released
    this->data_lines    =  0; // data lines released
    this->arb_winner_id = -1;
    this->initiator_id  = -1;
    this->target_id     = -1;
}

void ScsiBus::register_device(int id, ScsiDevice* dev_obj)
{
    if (this->devices[id] != nullptr) {
        ABORT_F("ScsiBus: device with ID %d already registered", id);
    }

    this->devices[id] = dev_obj;

    dev_obj->set_bus_object_ptr(this);
}

void ScsiBus::change_bus_phase(int initiator_id)
{
    for (int i = 0; i < SCSI_MAX_DEVS; i++) {
        if (i == initiator_id)
            continue; // don't notify the initiator
        if (this->devices[i] != nullptr) {
            this->devices[i]->notify(ScsiMsg::BUS_PHASE_CHANGE, this->cur_phase);
        }
    }
}

void ScsiBus::assert_ctrl_line(int initiator_id, uint16_t mask)
{
    DCHECK_F(initiator_id >= 0 && initiator_id < SCSI_MAX_DEVS, "ScsiBus: invalid initiator ID %d", initiator_id);

    uint16_t new_state = 0xFFFFU & mask;

    this->dev_ctrl_lines[initiator_id] |= new_state;

    if (new_state == this->ctrl_lines) {
        return;
    }

    if (new_state & SCSI_CTRL_RST) {
        this->ctrl_lines |= SCSI_CTRL_RST;
        this->cur_phase = ScsiPhase::RESET;
        change_bus_phase(initiator_id);
    }
}

void ScsiBus::release_ctrl_line(int id, uint16_t mask)
{
    DCHECK_F(id >= 0 && id < SCSI_MAX_DEVS, "ScsiBus: invalid initiator ID %d", id);

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
    } else {
        this->ctrl_lines = new_state;
    }
}

void ScsiBus::release_ctrl_lines(int id)
{
    this->release_ctrl_line(id, 0xFFFFUL);
}

uint16_t ScsiBus::test_ctrl_lines(uint16_t mask)
{
    uint16_t new_state = 0;

    // OR control lines of all devices together
    for (int i = 0; i < SCSI_MAX_DEVS; i++) {
        new_state |= this->dev_ctrl_lines[i];
    }

    return new_state & mask;
}

int ScsiBus::switch_phase(int id, int new_phase)
{
    int old_phase = this->cur_phase;

    // leave the current phase (low-level)
    switch (old_phase) {
    case ScsiPhase::COMMAND:
        this->release_ctrl_line(id, SCSI_CTRL_CD);
        break;
    case ScsiPhase::DATA_IN:
        this->release_ctrl_line(id, SCSI_CTRL_IO);
        break;
    case ScsiPhase::STATUS:
        this->release_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_IO);
        break;
    case ScsiPhase::MESSAGE_OUT:
        this->release_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG);
        break;
    case ScsiPhase::MESSAGE_IN:
        this->release_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG | SCSI_CTRL_IO);
        break;
    }

    // enter new phase (low-level)
    switch (new_phase) {
    case ScsiPhase::COMMAND:
        this->assert_ctrl_line(id, SCSI_CTRL_CD);
        break;
    case ScsiPhase::DATA_IN:
        this->assert_ctrl_line(id, SCSI_CTRL_IO);
        break;
    case ScsiPhase::STATUS:
        this->assert_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_IO);
        break;
    case ScsiPhase::MESSAGE_OUT:
        this->assert_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG);
        break;
    case ScsiPhase::MESSAGE_IN:
        this->assert_ctrl_line(id, SCSI_CTRL_CD | SCSI_CTRL_MSG | SCSI_CTRL_IO);
        break;
    }

    // switch the bus to the new phase (high-level)
    this->cur_phase = new_phase;
    change_bus_phase(id);

    return old_phase;
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

    this->assert_ctrl_line(initiator_id, SCSI_CTRL_SEL);

    this->data_lines = (1 << initiator_id) | (1 << target_id);

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
        this->devices[this->initiator_id]->notify(ScsiMsg::CONFIRM_SEL, target_id);
    }
}

bool ScsiBus::end_selection(int initiator_id, int target_id)
{
    // check for selection confirmation from target
    return this->target_id == target_id;
}

bool ScsiBus::pull_data(const int id, uint8_t* dst_ptr, const int size)
{
    if (dst_ptr == nullptr || !size) {
        return false;
    }

    if (!this->devices[id]->send_data(dst_ptr, size)) {
        LOG_F(ERROR, "ScsiBus: error while transferring T->I data!");
        return false;
    }

    return true;
}

bool ScsiBus::push_data(const int id, const uint8_t* src_ptr, const int size)
{
    if (!this->devices[id]->rcv_data(src_ptr, size)) {
        LOG_F(ERROR, "ScsiBus: error while transferring I->T data!");
        return false;
    }

    return true;
}

int ScsiBus::target_xfer_data() {
    return this->devices[this->target_id]->xfer_data();
}

void ScsiBus::target_next_step()
{
    this->devices[this->target_id]->next_step();
}

bool ScsiBus::negotiate_xfer(int& bytes_in, int& bytes_out)
{
    this->devices[this->target_id]->prepare_xfer(this, bytes_in, bytes_out);

    return true;
}

void ScsiBus::disconnect(int dev_id)
{
    this->release_ctrl_lines(dev_id);
    if (!(this->ctrl_lines & SCSI_CTRL_BSY) && !(this->ctrl_lines & SCSI_CTRL_SEL)) {
        this->cur_phase = ScsiPhase::BUS_FREE;
        change_bus_phase(dev_id);
    }
}

void ScsiBus::attach_scsi_devices(const std::string bus_suffix)
{
    std::string path;
    int scsi_id;
    std::string image_path;

    image_path = GET_STR_PROP("hdd_img" + bus_suffix);
    if (!image_path.empty()) {
        std::istringstream image_stream(image_path);
        while (std::getline(image_stream, path, ':')) {
            // do two passes because we skip ID 3.
            for (scsi_id = 0; scsi_id < SCSI_MAX_DEVS * 2 && (scsi_id == 3 || this->devices[scsi_id % SCSI_MAX_DEVS]); scsi_id++) {}
            if (scsi_id < SCSI_MAX_DEVS * 2) {
                scsi_id = scsi_id % SCSI_MAX_DEVS;
                std::string scsi_device_name = "ScsiHD" + bus_suffix + "," + std::to_string(scsi_id);
                ScsiHardDisk *scsi_device = new ScsiHardDisk(scsi_device_name, scsi_id);
                gMachineObj->add_device(scsi_device_name, std::unique_ptr<ScsiHardDisk>(scsi_device));
                this->register_device(scsi_id, scsi_device);
                scsi_device->insert_image(path);
            }
            else {
                LOG_F(ERROR, "%s: Too many devices. HDD \"%s\" was not added.", this->get_name().c_str(), path.c_str());
            }
        }
    }

    image_path = GET_STR_PROP("cdr_img" + bus_suffix);
    if (!image_path.empty()) {
        std::istringstream image_stream(image_path);
        while (std::getline(image_stream, path, ':')) {
             // do two passes because we start at ID 3.
            for (scsi_id = 3; scsi_id < SCSI_MAX_DEVS * 2 && this->devices[scsi_id % SCSI_MAX_DEVS]; scsi_id++) {}
            if (scsi_id < SCSI_MAX_DEVS * 2) {
                scsi_id = scsi_id % SCSI_MAX_DEVS;
                std::string scsi_device_name = "ScsiCdrom" + bus_suffix + "," + std::to_string(scsi_id);
                ScsiCdrom *scsi_device = new ScsiCdrom(scsi_device_name, scsi_id);
                gMachineObj->add_device(scsi_device_name, std::unique_ptr<ScsiCdrom>(scsi_device));
                this->register_device(scsi_id, scsi_device);
                scsi_device->insert_image(path);
            }
            else {
                LOG_F(ERROR, "%s: Too many devices. CD-ROM \"%s\" was not added.", this->get_name().c_str(), path.c_str());
            }
        }
    }
}

static const DeviceDescription ScsiCurio_Descriptor = {
    ScsiBus::create_ScsiCurio, {}, {}
};

static const DeviceDescription ScsiMesh_Descriptor = {
    ScsiBus::create_ScsiMesh, {}, {}
};

REGISTER_DEVICE(ScsiCurio, ScsiCurio_Descriptor);
REGISTER_DEVICE(ScsiMesh, ScsiMesh_Descriptor);
