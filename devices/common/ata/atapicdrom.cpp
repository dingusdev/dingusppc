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
#include <devices/common/ata/idechannel.h>
#include <devices/common/scsi/scsi.h> // ATAPI CDROM reuses SCSI commands (sic!)
#include <devices/deviceregistry.h>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <string>

using namespace ata_interface;

AtapiCdrom::AtapiCdrom(std::string name) : CdromDrive(), AtapiBaseDevice(name) {
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
    this->sector_areas = 0;
    if (this->doing_sector_areas) {
        this->doing_sector_areas = false;
        LOG_F(WARNING, "%s: doing_sector_areas reset", this->name.c_str());
    }

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
    case ScsiCommand::MODE_SENSE_6:
        this->xfer_cnt = this->mode_sense_ex(true, this->cmd_pkt, this->data_buf);
        if (!this->xfer_cnt) {
            this->present_status();
        } else {
            this->r_byte_count = this->xfer_cnt;
            this->data_ptr     = (uint16_t*)this->data_buf;
            this->status_good();
            this->data_out_phase();
        }
        break;
    case ScsiCommand::MODE_SENSE_10:
        this->xfer_cnt = this->mode_sense_ex(false, this->cmd_pkt, this->data_buf);
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
    {
        lba = READ_DWORD_BE_U(&this->cmd_pkt[2]);
        xfer_len = (this->cmd_pkt[6] << 16) | READ_WORD_BE_U(&this->cmd_pkt[7]);
        if (this->cmd_pkt[1] || (this->cmd_pkt[9] & ~0xf8) || ((this->cmd_pkt[9] & 0xf8) == 0) || this->cmd_pkt[10])
            LOG_F(WARNING, "%s: unsupported READ_CD params: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                this->name.c_str(),
                this->cmd_pkt[0], this->cmd_pkt[1], this->cmd_pkt[2], this->cmd_pkt[3], this->cmd_pkt[4], this->cmd_pkt[5],
                this->cmd_pkt[6], this->cmd_pkt[7], this->cmd_pkt[8], this->cmd_pkt[9], this->cmd_pkt[10], this->cmd_pkt[11]
            );
        if (this->r_features & ATAPI_Features::DMA) {
            LOG_F(WARNING, "ATAPI DMA transfer requsted");
        }
        this->set_fpos(lba);
        this->sector_areas = cmd_pkt[9];
        this->doing_sector_areas = true;
        this->current_block = lba;
        this->current_block_byte = 0;

        int bytes_prolog = 0;
        int bytes_data = 0;
        int bytes_epilog = 0;

        // For Mode 1 CD-ROMs:
        if (this->sector_areas & 0x80) bytes_prolog +=   12; // Sync
        if (this->sector_areas & 0x20) bytes_prolog +=    4; // Header
        if (this->sector_areas & 0x40) bytes_prolog +=    0; // SubHeader
        if (this->sector_areas & 0x10) bytes_data   += 2048; // User
        if (this->sector_areas & 0x08) bytes_epilog +=  288; // Auxiliary
        if (this->sector_areas & 0x02) bytes_epilog +=  294; // ErrorFlags
        if (this->sector_areas & 0x01) bytes_epilog +=   96; // SubChannel

        int bytes_per_block = bytes_prolog + bytes_data + bytes_epilog;

        int disk_image_byte_count;

        if (bytes_per_block == 0) {
            disk_image_byte_count = 0xffff;
        }
        else {
            disk_image_byte_count = (this->r_byte_count / bytes_per_block) * this->block_size; // whole blocks
            if ((this->r_byte_count % bytes_per_block) > bytes_prolog) { // partial block
                int disk_image_byte_count_partial_block = (this->r_byte_count % bytes_per_block) - bytes_prolog; // remove prolog from partial block
                if (disk_image_byte_count_partial_block > this->block_size) { // partial block includes some epilog?
                    disk_image_byte_count_partial_block = this->block_size; // // remove epilog from partial block
                }
                // add partial block
                disk_image_byte_count += disk_image_byte_count_partial_block;
            }
        }

        int disk_image_bytes_received = this->read_begin(xfer_len, disk_image_byte_count);

        int bytes_received = (disk_image_bytes_received / this->block_size) * bytes_per_block + bytes_prolog + (disk_image_bytes_received % this->block_size); // whole blocks + prolog + partial block
        if (bytes_received > this->r_byte_count) { // if partial epilog or partial prolog
            bytes_received = this->r_byte_count; // confine to r_byte_count
        }
        this->r_byte_count = bytes_received;
        this->xfer_cnt = bytes_received;

        this->data_ptr = (uint16_t*)this->data_cache.get();
        this->status_good();
        this->data_out_phase();
        break;
    }
    default:
        LOG_F(ERROR, "%s: unsupported ATAPI command 0x%X", this->name.c_str(),
              this->cmd_pkt[0]);
        this->status_error(ScsiSense::ILLEGAL_REQ, ScsiError::INVALID_CMD);
        this->present_status();
    }
}

int AtapiCdrom::request_data() {
    // continuation of READ_CD above

    this->data_ptr = (uint16_t*)this->data_cache.get();

    this->xfer_cnt = this->read_more();

    return this->xfer_cnt;
}

static const uint8_t mode_1_sync[] = { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };

uint16_t AtapiCdrom::get_data() {
    uint16_t ret_data;

    if (doing_sector_areas) {
        // For Mode 1 CD-ROMs:
        int area_start;
        int area_end = 0;

        ret_data = 0xffff;

        if (this->sector_areas & 0x80) {
            area_start = area_end;
            area_end += 12; // Sync
            if (this->current_block_byte >= area_start && this->current_block_byte < area_end) {
                ret_data = BYTESWAP_16(*((uint16_t*)(&mode_1_sync[current_block_byte - area_start])));
            }
        }

        if (this->sector_areas & 0x20) {
            area_start = area_end;
            area_end += 4; // Header
            if (this->current_block_byte >= area_start && this->current_block_byte < area_end) {
                AddrMsf msf = lba_to_msf(this->current_block + 150);
                uint8_t header[4];
                header[0] = msf.min;
                header[1] = msf.sec;
                header[2] = msf.frm;
                header[3] = 0x01; // Mode 1
                ret_data = BYTESWAP_16(*((uint16_t*)(&header[current_block_byte - area_start])));
            }
        }

        if (this->sector_areas & 0x40) {
            area_start = area_end;
            area_end += 0; // SubHeader
            if (this->current_block_byte >= area_start && this->current_block_byte < area_end) {
                ret_data = 0;
            }
        }

        if (this->sector_areas & 0x10) {
            area_start = area_end;
            area_end += 2048; // User
            if (this->current_block_byte >= area_start && this->current_block_byte < area_end) {
                ret_data = AtaBaseDevice::get_data();
            }
        }

        if (this->sector_areas & 0x08) {
            area_start = area_end;
            area_end += 288; // Auxiliary
            if (this->current_block_byte >= area_start && this->current_block_byte < area_end) {
                ret_data = 0;
            }
        }

        if (this->sector_areas & 0x02) {
            area_start = area_end;
            area_end += 294; // ErrorFlags
            if (this->current_block_byte >= area_start && this->current_block_byte < area_end) {
                ret_data = 0;
            }
        }

        if (this->sector_areas & 0x01) {
            area_start = area_end;
            area_end += 96; // SubChannel
            if (this->current_block_byte >= area_start && this->current_block_byte < area_end) {
                ret_data = 0;
            }
        }

        current_block_byte += 2;
        if (current_block_byte >= area_end)
            current_block_byte = 0;
    }
    else {
        ret_data = AtaBaseDevice::get_data();
    }

    return ret_data;
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
