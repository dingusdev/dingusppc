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

/** @file ATA hard disk emulation. */

#include <devices/common/ata/atahd.h>
#include <devices/deviceregistry.h>
#include <devices/common/ata/idechannel.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cstring>
#include <fstream>
#include <string>

using namespace ata_interface;

AtaHardDisk::AtaHardDisk(std::string name) : AtaBaseDevice(name, DEVICE_TYPE_ATA) {
}

int AtaHardDisk::device_postinit() {
    std::string hdd_config = GET_STR_PROP("hdd_config");
    if (hdd_config.empty()) {
        LOG_F(ERROR, "%s: hdd_config property is empty", this->name.c_str());
        return -1;
    }

    std::string bus_id;
    uint32_t    dev_num;

    parse_device_path(hdd_config, bus_id, dev_num);

    auto bus_obj = dynamic_cast<IdeChannel*>(gMachineObj->get_comp_by_name(bus_id));
    bus_obj->register_device(dev_num, this);

    std::string hdd_image_path = GET_STR_PROP("hdd_img");
    if (!hdd_image_path.empty()) {
        this->insert_image(hdd_image_path);
    }

    return 0;
}

void AtaHardDisk::insert_image(std::string filename) {
    if (!this->hdd_img.open(filename)) {
        ABORT_F("%s: could not open image file", this->name.c_str());
    }

    this->img_size = this->hdd_img.size();
    this->total_sectors = this->hdd_img.size() / ATA_HD_SEC_SIZE;
}

int AtaHardDisk::perform_command() {
    this->r_status |= BSY;
    this->r_error = 0;

    switch (this->r_command) {
    case NOP:
        break;
    case RECALIBRATE:
        this->r_error       = 0;
        this->r_cylinder_lo = 0;
        this->r_cylinder_hi = 0;
        break;
    case READ_SECTOR:
    case READ_SECTOR_NR: {
            uint16_t sec_count = this->r_sect_count ? this->r_sect_count : 256;
            int      xfer_size = sec_count * ATA_HD_SEC_SIZE;
            uint64_t offset    = this->get_lba() * ATA_HD_SEC_SIZE;
            hdd_img.read(buffer, offset, xfer_size);
            this->data_ptr = (uint16_t *)this->buffer;
            // those commands should generate IRQ for each sector
            this->prepare_xfer(xfer_size, ATA_HD_SEC_SIZE);
            this->signal_data_ready();
        }
        break;
    case WRITE_SECTOR:
    case WRITE_SECTOR_NR: {
            uint16_t sec_count = this->r_sect_count ? this->r_sect_count : 256;
            this->cur_fpos = this->get_lba() * ATA_HD_SEC_SIZE;
            this->data_ptr = (uint16_t *)this->buffer;
            this->cur_data_ptr = this->data_ptr;
            this->prepare_xfer(sec_count * ATA_HD_SEC_SIZE, ATA_HD_SEC_SIZE);
            this->post_xfer_action = [this]() {
                this->hdd_img.write(this->data_ptr, this->cur_fpos, this->chunk_size);
                this->cur_fpos += this->chunk_size;
            };
            this->r_status |= DRQ;
            this->r_status &= ~BSY;
        }
        break;
    case INIT_DEV_PARAM:
        // update fictive disk geometry with parameters from host
        this->sectors = this->r_sect_count;
        this->heads   = (this->r_dev_head & 0xF) + 1;
        this->r_status &= ~BSY;
        this->update_intrq(1);
        break;
    case DIAGNOSTICS:
        this->r_error = 1;
        this->device_set_signature();
        break;
    case FLUSH_CACHE: // used by the XNU kernel driver
        this->r_status &= ~(BSY | DRQ | ERR);
        this->update_intrq(1);
        break;
    case IDENTIFY_DEVICE:
        this->prepare_identify_info();
        this->data_ptr = (uint16_t *)this->data_buf;
        this->prepare_xfer(ATA_HD_SEC_SIZE, ATA_HD_SEC_SIZE);
        this->signal_data_ready();
        break;
    case SET_FEATURES:
        if (this->r_features == 3) {
            switch(this->r_sect_count >> 3) {
            case 0:
                LOG_F(INFO, "%s: default transfer mode requested", this->name.c_str());
                break;
            case 1:
                LOG_F(INFO, "%s: PIO transfer mode set to 0x%X", this->name.c_str(),
                      this->r_sect_count & 7);
                break;
            case 4:
                LOG_F(INFO, "%s: Multiword DMA mode set to 0x%X", this->name.c_str(),
                      this->r_sect_count & 7);
                break;
            default:
                LOG_F(ERROR, "%s: unsupported transfer mode 0x%X", this->name.c_str(),
                      this->r_sect_count);
                this->r_error  |= ATA_Error::ABRT;
                this->r_status |= ATA_Status::ERR;
            }
        } else {
            LOG_F(WARNING, "%s: unsupported SET_FEATURES subcommand code 0x%X",
                  this->name.c_str(), this->r_features);
        }
        this->r_status &= ~BSY;
        this->update_intrq(1);
        break;
    default:
        LOG_F(ERROR, "%s: unknown ATA command 0x%x", this->name.c_str(), this->r_command);
        this->r_status &= ~BSY;
        this->r_status |= ERR;
        return -1;
    }
    return 0;
}

void AtaHardDisk::prepare_identify_info() {
    uint16_t    *buf_ptr = (uint16_t *)this->data_buf;

    std::memset(this->data_buf, 0, sizeof(this->data_buf));

    buf_ptr[ 0] = 0x0040; // ATA device, non-removable media, non-removable drive
    buf_ptr[49] = 0x200; // report LBA support

    // TODO: replace this fictive CHS geometry with the proper one
    buf_ptr[ 1] = 965;
    buf_ptr[ 3] = 5;
    buf_ptr[ 6] = 17;

    // report LBA capacity
    WRITE_WORD_LE_A(&buf_ptr[60], (this->total_sectors & 0xFFFFU));
    WRITE_WORD_LE_A(&buf_ptr[61], (this->total_sectors >> 16) & 0xFFFFU);
}

uint64_t AtaHardDisk::get_lba() {
    if (this->r_dev_head & ATA_Dev_Head::LBA) {
        return ((this->r_dev_head & 0xF) << 24) | (this->r_cylinder_hi << 16) |
                (this->r_cylinder_lo << 8) | (this->r_sect_num);
    } else { // translate old fashioned CHS addressing to LBA
        uint16_t c = (this->r_cylinder_hi << 8) + this->r_cylinder_lo;
        uint8_t  h = this->r_dev_head & 0xF;
        uint8_t  s = this->r_sect_num;

        if (!s) {
            LOG_F(ERROR, "%s: zero sector number is not allowed!", this->name.c_str());
            return -1ULL;
        } else
            return (this->heads * c + h) * this->sectors + s - 1;
    }
}

static const PropMap AtaHardDiskProperties = {
    {"hdd_img", new StrProperty("")},
};

static const DeviceDescription AtaHardDiskDescriptor =
    {AtaHardDisk::create, {}, AtaHardDiskProperties};

REGISTER_DEVICE(AtaHardDisk, AtaHardDiskDescriptor);
