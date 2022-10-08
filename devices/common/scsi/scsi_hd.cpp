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

/** @file Sander-Wozniak Machine 3 (SWIM3) emulation. */

#include <fstream>
#include <limits>
#include <stdio.h>
#include <devices/deviceregistry.h>
#include <devices/common/scsi/scsi_hd.h>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>
#include <devices/common/scsi/sc53c94.h>

#define sector_size 512

using namespace std;

SCSI_HD::SCSI_HD() {
    supports_types(HWCompType::SCSI_DEV);

    std::string hd_image_path = GET_STR_PROP("hdd_img");

    //We don't want to store everything in memory, but
    //we want to keep the hard disk available.
    this->hdd_img.open(hd_image_path, ios::out | ios::in | ios::binary);

    // Taken from:
    // https://stackoverflow.com/questions/22984956/tellg-function-give-wrong-size-of-file/22986486
    this->hdd_img.ignore(std::numeric_limits<std::streamsize>::max());
    this->img_size = this->hdd_img.gcount();
    this->hdd_img.clear();    //  Since ignore will have set eof.
    this->hdd_img.seekg(0, std::ios_base::beg);
}

int SCSI_HD::test_unit_ready() {
    return 0x0;
}

int SCSI_HD::req_sense(uint8_t alloc_len) {
    if (alloc_len != 252) {
        LOG_F(WARNING, "Inappropriate Allocation Length: %%d", alloc_len);
    }
    return 0x0; //placeholder - no sense
}

int SCSI_HD::inquiry() {
    return 0x1000000F;
}

int SCSI_HD::send_diagnostic() {
    return 0x0;
}

int SCSI_HD::read_capacity_10() {
    return this->img_size;
}

void SCSI_HD::format() {}

void SCSI_HD::read(uint32_t lba, uint16_t transfer_len) {
    uint32_t transfer_size  = (transfer_len == 0) ? 256 : transfer_len;
    transfer_size           *= sector_size;
    uint32_t device_offset  = lba * sector_size;

    this->hdd_img.seekg(device_offset, this->hdd_img.beg);
    this->hdd_img.read(img_buffer, transfer_size);
}

void SCSI_HD::write(uint32_t lba, uint16_t transfer_len) {
    uint32_t transfer_size = (transfer_len == 0) ? 256 : transfer_len;
    transfer_size *= sector_size;
    uint32_t device_offset = lba * sector_size;

    this->hdd_img.seekg(device_offset, this->hdd_img.beg);
    this->hdd_img.write((char*)&img_buffer, transfer_size);

}

void SCSI_HD::seek(uint32_t lba) {
    this->hdd_img.seekg(0, this->hdd_img.beg);
}

void SCSI_HD::rewind() {
    this->hdd_img.seekg(0, this->hdd_img.beg);
}

static const PropMap SCSI_HD_Properties = {
    {"hdd_img", new StrProperty("")},
    {"hdd_wr_prot", new BinProperty(0)},
};