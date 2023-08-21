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

/** @file ATAPI CDROM implementation. */

#include <devices/common/ata/atadefs.h>
#include <devices/common/ata/atapicdrom.h>
#include <devices/common/scsi/scsi.h> // ATAPI CDROM reuses SCSI commands (sic!)
#include <devices/deviceregistry.h>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <string>

using namespace ata_interface;

AtapiCdrom::AtapiCdrom(std::string name) : AtapiBaseDevice(name) {
    this->set_error_callback(
        [this](uint8_t sense_key, uint8_t asc) {
            this->status_error(sense_key, asc);
        }
    );
}

int AtapiCdrom::device_postinit() {
    std::string cdr_config = GET_STR_PROP("cdr_config");
    if (cdr_config.empty()) {
        LOG_F(ERROR, "%s: cdr_config property is empty", this->name.c_str());
        return -1;
    }

    std::string bus_id;
    uint32_t    dev_num;

    parse_device_path(cdr_config, bus_id, dev_num);

    auto bus_obj = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name(bus_id));
    bus_obj->register_device(dev_num, this);

    std::string cdr_image_path = GET_STR_PROP("cdr_img");
    if (!cdr_image_path.empty()) {
        this->insert_image(cdr_image_path);
    }

    return 0;
}

void AtapiCdrom::perform_packet_command() {
    uint32_t lba, xfer_len;

    this->r_status |= BSY;

    switch (this->cmd_pkt[0]) {
    case ScsiCommand::TEST_UNIT_READY:
        if (this->medium_present())
            this->status_good();
        else
            this->status_error(ScsiSense::NOT_READY, ScsiError::MEDIUM_NOT_PRESENT);
        this->present_status();
        break;
    case ScsiCommand::REQ_SENSE:
        xfer_len = this->request_sense(this->data_buf, this->sense_key, this->asc, this->ascq);
        if (!xfer_len) {
            this->present_status();
        } else {
            this->xfer_cnt = std::min((uint32_t)this->r_byte_count, xfer_len);
            this->data_ptr = (uint16_t*)this->data_buf;
            this->status_good();
            this->data_out_phase();
        }
        break;
    case ScsiCommand::INQUIRY:
        this->xfer_cnt = this->inquiry(this->cmd_pkt, this->data_buf);
        this->r_byte_count = this->xfer_cnt;
        this->data_ptr = (uint16_t*)this->data_buf;
        this->status_good();
        this->data_out_phase();
        break;
    case ScsiCommand::START_STOP_UNIT:
        if ((this->cmd_pkt[4] & 3) == 2) {
            LOG_F(WARNING, "CD-ROM eject requested");
        }
        this->status_good();
        this->present_status();
        break;
    case ScsiCommand::PREVENT_ALLOW_MEDIUM_REMOVAL:
        LOG_F(INFO, "%s: medium removal %s", this->name.c_str(),
            this->cmd_pkt[4] & 1 ? "prevented" : "allowed");
        this->status_good();
        this->present_status();
        break;
    case ScsiCommand::READ_CAPACITY_10:
        this->xfer_cnt = this->report_capacity(this->data_buf);
        this->r_byte_count = this->xfer_cnt;
        this->data_ptr = (uint16_t*)this->data_buf;
        this->status_good();
        this->data_out_phase();
        break;
    case ScsiCommand::READ_TOC:
        this->status_good();
        xfer_len = this->read_toc(this->cmd_pkt, this->data_buf);
        if (!xfer_len) {
            this->present_status();
        } else {
            this->xfer_cnt = std::min((uint32_t)this->r_byte_count, xfer_len);
            this->data_ptr = (uint16_t*)this->data_buf;
            this->data_out_phase();
        }
        break;
    case ScsiCommand::MODE_SENSE_10:
        this->xfer_cnt = this->mode_sense_ex(this->cmd_pkt, this->data_buf);
        if (!this->xfer_cnt) {
            this->present_status();
        } else {
            this->r_byte_count = this->xfer_cnt;
            this->data_ptr = (uint16_t*)this->data_buf;
            this->status_good();
            this->data_out_phase();
        }
        break;
    case ScsiCommand::READ_6:
        lba      = this->cmd_pkt[1] << 16 | READ_WORD_BE_U(&this->cmd_pkt[2]);
        xfer_len = this->cmd_pkt[4];
        if (this->r_features & ATAPI_Features::DMA) {
            LOG_F(WARNING, "ATAPI DMA transfer requsted");
        }
        this->set_fpos(lba);
        this->xfer_cnt = this->read_begin(xfer_len, this->r_byte_count);
        this->r_byte_count = this->xfer_cnt;
        this->data_ptr = (uint16_t*)this->data_cache.get();
        this->status_good();
        this->data_out_phase();
        break;
    case ScsiCommand::READ_10:
        lba      = READ_DWORD_BE_U(&this->cmd_pkt[2]);
        xfer_len = READ_WORD_BE_U(&this->cmd_pkt[7]);
        if (this->r_features & ATAPI_Features::DMA) {
            LOG_F(WARNING, "ATAPI DMA transfer requsted");
        }
        this->set_fpos(lba);
        this->xfer_cnt = this->read_begin(xfer_len, this->r_byte_count);
        this->r_byte_count = this->xfer_cnt;
        this->data_ptr = (uint16_t*)this->data_cache.get();
        this->status_good();
        this->data_out_phase();
        break;
    case ScsiCommand::READ_12:
        lba      = READ_DWORD_BE_U(&this->cmd_pkt[2]);
        xfer_len = READ_DWORD_BE_U(&this->cmd_pkt[6]);
        if (this->r_features & ATAPI_Features::DMA) {
            LOG_F(WARNING, "ATAPI DMA transfer requsted");
        }
        this->set_fpos(lba);
        this->xfer_cnt = this->read_begin(xfer_len, this->r_byte_count);
        this->r_byte_count = this->xfer_cnt;
        this->data_ptr = (uint16_t*)this->data_cache.get();
        this->status_good();
        this->data_out_phase();
        break;
    case ScsiCommand::SET_CD_SPEED:
        LOG_F(INFO, "%s: speed set to %d kBps", this->name.c_str(),
              READ_WORD_BE_U(&this->cmd_pkt[2]));
        this->status_good();
        this->present_status();
        break;
    case ScsiCommand::READ_CD:
        lba      = READ_DWORD_BE_U(&this->cmd_pkt[2]);
        xfer_len = (this->cmd_pkt[6] << 16) | READ_WORD_BE_U(&this->cmd_pkt[7]);
        if (this->cmd_pkt[1] || this->cmd_pkt[9] != 0x10 || this->cmd_pkt[10])
            LOG_F(WARNING, "%s: unsupported READ_CD params", this->name.c_str());
        if (this->r_features & ATAPI_Features::DMA) {
            LOG_F(WARNING, "ATAPI DMA transfer requsted");
        }
        this->set_fpos(lba);
        this->xfer_cnt = this->read_begin(xfer_len, this->r_byte_count);
        this->r_byte_count = this->xfer_cnt;
        this->data_ptr = (uint16_t*)this->data_cache.get();
        this->status_good();
        this->data_out_phase();
        break;
    default:
        LOG_F(ERROR, "%s: unsupported ATAPI command 0x%X", this->name.c_str(),
              this->cmd_pkt[0]);
        this->status_error(ScsiSense::ILLEGAL_REQ, ScsiError::INVALID_CMD);
        this->present_status();
    }
}

void AtapiCdrom::status_good() {
    this->status_expected = true;
    this->r_error = 0;
    this->sense_key = 0;
    this->r_status &= ~ATA_Status::ERR;
}

void AtapiCdrom::status_error(uint8_t sense_key, uint8_t asc) {
    this->status_expected = true;
    this->sense_key = sense_key;
    this->asc = asc;
    this->r_error = (sense_key << 4) | ATA_Error::ABRT;
    this->r_status |= ATA_Status::ERR;
}

static const PropMap AtapiCdromProperties = {
    {"cdr_img", new StrProperty("")},
};

static const DeviceDescription AtapiCdromDescriptor =
    {AtapiCdrom::create, {}, AtapiCdromProperties};

REGISTER_DEVICE(AtapiCdrom, AtapiCdromDescriptor);
