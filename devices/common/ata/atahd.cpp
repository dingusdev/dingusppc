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

/** @file ATA hard disk emulation. */

#include <devices/common/ata/atahd.h>
#include <sys/stat.h>
#include <fstream>
#include <string>
#include <loguru.hpp>

using namespace ata_interface;

AtaHardDisk::AtaHardDisk() : AtaBaseDevice("ATA-HD")
{
}

void AtaHardDisk::insert_image(std::string filename) {
    this->hdd_img.open(filename, std::fstream::out | std::fstream::in | std::fstream::binary);

    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    if (!rc) {
        this->img_size = stat_buf.st_size;
    } else {
        ABORT_F("AtaHardDisk: could not determine file size using stat()");
    }
    this->hdd_img.seekg(0, std::ios_base::beg);
}

int AtaHardDisk::perform_command()
{
    LOG_F(INFO, "%s: command 0x%x requested", this->name.c_str(), this->r_command);
    this->r_status |= BSY;
    this->r_status &= ~(DRDY);
    switch (this->r_command) {
        case NOP:
            break;
        case RESET_ATAPI: {
            device_reset();
            break;
        }
        case RECALIBRATE:
            hdd_img.seekg(0, std::ios::beg);
            this->r_error       = 0;
            this->r_cylinder_lo = 0;
            this->r_cylinder_hi = 0;
            break;
        case READ_SECTOR:
        case READ_SECTOR_NR: {
            this->r_status |= DRQ;
            uint16_t sec_count = (this->r_sect_count == 0) ? 256 : this->r_sect_count;
            uint32_t sector = (r_sect_num << 16);
            sector |= ((this->r_cylinder_lo) << 8) + (this->r_cylinder_hi);
            uint64_t offset = sector * 512;
            hdd_img.seekg(offset, std::ios::beg);
            hdd_img.read(buffer, sec_count * 512);
            this->r_status &= ~(DRQ);
            break;
        }
        case WRITE_SECTOR:
        case WRITE_SECTOR_NR: {
            this->r_status |= DRQ;
            uint16_t sec_count = (this->r_sect_count == 0) ? 256 : this->r_sect_count;
            uint32_t sector    = (r_sect_num << 16);
            sector |= ((this->r_cylinder_lo) << 8) + (this->r_cylinder_hi);
            uint64_t offset = sector * 512;
            hdd_img.seekg(offset, std::ios::beg);
            hdd_img.write(buffer, sec_count * 512);
            this->r_status &= ~(DRQ);
            break;
        }
        case INIT_DEV_PARAM:
            break;
        case DIAGNOSTICS: {
            this->r_status |= DRQ;
            int ret_code = this->r_error;
            this->r_status &= ~(DRQ);
            return ret_code;
            break;
        }
        case IDENTIFY_DEVICE: {
            this->r_status |= DRQ;
            memcpy(buffer, ide_hd_id, 512);
            this->r_status &= ~(DRQ);
            break;
        }
        default:
            LOG_F(INFO, "Unknown ATA command 0x%x", this->r_command);
            this->r_status &= ~(BSY);
            this->r_status |= ERR;
            return -1;
    }
    this->r_status &= ~(BSY);
    this->r_status |= DRDY;
    return 0;
}
