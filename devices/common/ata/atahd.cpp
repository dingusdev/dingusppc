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
#include <machines/machinebase.h>

#include <cstring>
#include <fstream>
#include <string>
#include <loguru.hpp>

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
}

int AtaHardDisk::perform_command()
{
    LOG_F(INFO, "%s: command 0x%x requested", this->name.c_str(), this->r_command);
    this->r_status |= BSY;

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
        this->xfer_cnt = sec_count * ATA_HD_SEC_SIZE;
        uint64_t offset = this->get_lba() * ATA_HD_SEC_SIZE;
        hdd_img.read(buffer, offset, this->xfer_cnt);
        this->data_ptr = (uint16_t *)this->buffer;
        this->signal_data_ready();
    }
    break;
    case WRITE_SECTOR:
    case WRITE_SECTOR_NR: {
        uint16_t sec_count = this->r_sect_count ? this->r_sect_count : 256;
        uint64_t offset = this->get_lba() * ATA_HD_SEC_SIZE;
        hdd_img.write(buffer, offset, sec_count * ATA_HD_SEC_SIZE);
    }
    break;
    case INIT_DEV_PARAM:
        // update fictive disk geometry with parameters from host
        this->sectors = this->r_sect_count;
        this->heads   = (this->r_dev_head & 0xF) + 1;
        this->r_status &= ~BSY;
        this->update_intrq(1);
        break;
    case DIAGNOSTICS: {
        this->r_status |= DRQ;
        int ret_code = this->r_error;
        this->r_status &= ~DRQ;
        return ret_code;
    }
    break;
    case IDENTIFY_DEVICE:
        this->prepare_identify_info();
        this->data_ptr = (uint16_t *)this->data_buf;
        this->xfer_cnt = ATA_HD_SEC_SIZE;
        this->signal_data_ready();
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

    buf_ptr[0] = 0x0040; // ATA device, non-removable media, non-removable drive
    buf_ptr[1] = 965;
    buf_ptr[3] = 5;
    buf_ptr[6] = 17;
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
