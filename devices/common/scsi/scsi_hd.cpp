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

/** @file Generic SCSI Hard Disk emulation. */

#include <devices/deviceregistry.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsi_hd.h>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>
#include <loguru.hpp>

#include <fstream>
#include <limits>
#include <cstring>
#include <stdio.h>

#define sector_size 512

using namespace std;

ScsiHardDisk::ScsiHardDisk(int my_id) : ScsiDevice(my_id) {
    supports_types(HWCompType::SCSI_DEV);
}
 
 void ScsiHardDisk::insert_image(std::string filename) {

    //We don't want to store everything in memory, but
    //we want to keep the hard disk available.
     this->hdd_img.open(filename, ios::out | ios::in | ios::binary);

    // Taken from:
    // https://stackoverflow.com/questions/22984956/tellg-function-give-wrong-size-of-file/22986486
    this->hdd_img.ignore(std::numeric_limits<std::streamsize>::max());
    this->img_size = this->hdd_img.gcount();
    this->hdd_img.clear();    //  Since ignore will have set eof.
    this->hdd_img.seekg(0, std::ios_base::beg);
}

void ScsiHardDisk::process_command() {
    uint32_t lba          = 0;
    uint16_t transfer_len = 0;
    uint16_t alloc_len    = 0;
    uint8_t  param_len    = 0;

    uint8_t  page_code    = 0;
    uint8_t  subpage_code = 0;

    uint8_t* cmd = this->cmd_buf;

    switch (cmd[0]) {
    case ScsiCommand::TEST_UNIT_READY:
        test_unit_ready();
        break;
    case ScsiCommand::REWIND:
        rewind();
        break;
    case ScsiCommand::REQ_SENSE:
        alloc_len = cmd[4];
        req_sense(alloc_len);
        break;
    case ScsiCommand::INQUIRY:
        alloc_len = (cmd[3] << 8) + cmd[4];
        inquiry(alloc_len);
        break;
    case ScsiCommand::READ_6:
        lba = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        transfer_len = cmd[4];
        read(lba, transfer_len, 6);
        break;
    case ScsiCommand::READ_10:
        lba          = (cmd[2] << 24) + (cmd[3] << 16) + (cmd[4] << 8) + cmd[5];
        transfer_len = (cmd[7] << 8) + cmd[8];
        read(lba, transfer_len, 10);
        break;
    case ScsiCommand::WRITE_6:
        lba          = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        transfer_len = cmd[4];
        write(lba, transfer_len, 6);
        break;
    case ScsiCommand::WRITE_10:
        lba          = (cmd[2] << 24) + (cmd[3] << 16) + (cmd[4] << 8) + cmd[5];
        transfer_len = (cmd[7] << 8) + cmd[8];
        write(lba, transfer_len, 10);
        break;
    case ScsiCommand::SEEK_6:
        lba         = ((cmd[1] & 0x1F) << 16) + (cmd[2] << 8) + cmd[3];
        seek(lba);
        break;
    case ScsiCommand::MODE_SELECT_6:
        param_len = cmd[4];
        mode_select_6(param_len);
        break;
    case ScsiCommand::MODE_SENSE_6:
        page_code    = cmd[2] & 0x1F;
        subpage_code = cmd[3];
        alloc_len    = cmd[4];
        mode_sense_6(page_code, subpage_code, alloc_len);
        break;
    case ScsiCommand::READ_CAPAC_10:
        read_capacity_10();
        break;
    default:
        LOG_F(WARNING, "SCSI_HD: unrecognized command: %x", cmd[0]);
    }
}

int ScsiHardDisk::test_unit_ready() {
    if (img_path.empty() || img_path == " ") {
        return ScsiError::DEV_NOT_READY;
    } else {
        return ScsiError::NO_ERROR;
    }
}

int ScsiHardDisk::req_sense(uint16_t alloc_len) {
    if (alloc_len != 252) {
        LOG_F(WARNING, "Inappropriate Allocation Length: %d", alloc_len);
    }
    return ScsiError::NO_ERROR;    // placeholder - no sense
}

void ScsiHardDisk::inquiry(uint16_t alloc_len) {
    if (alloc_len >= 48) {
        uint8_t empty_filler[1 << 17] = {0x0};
        std::memcpy(img_buffer, empty_filler, (1 << 17));
        img_buffer[2] = 0x1;
        img_buffer[3] = 0x2;
        img_buffer[4] = 0x31;
        img_buffer[7] = 0x1C;
        std::memcpy(img_buffer + 8, vendor_info, 8);
        std::memcpy(img_buffer + 16, prod_info, 16);
        std::memcpy(img_buffer + 32, rev_info, 8);
        std::memcpy(img_buffer + 40, serial_info, 8);
    }
    else {
        LOG_F(WARNING, "Inappropriate Allocation Length: %d", alloc_len);
    }
}

int ScsiHardDisk::send_diagnostic() {
    return 0x0;
}

int ScsiHardDisk::mode_select_6(uint8_t param_len) {
    if (param_len == 0) {
        return 0x0;
    }
    else {
        LOG_F(WARNING, "Mode Select calling for param length of: %d", param_len);
        return param_len;
    }
}

void ScsiHardDisk::mode_sense_6(uint8_t page_code, uint8_t subpage_code, uint8_t alloc_len) {
    LOG_F(WARNING, "Page Code %d; Subpage Code: %d", page_code, subpage_code);
}

void ScsiHardDisk::read_capacity_10() {
    uint32_t sec_limit            = (this->img_size >> 9);

    std::memset(img_buffer, 0, sizeof(img_buffer));

    img_buffer[0] = (sec_limit >> 24) & 0xFF;
    img_buffer[1] = (sec_limit >> 16) & 0xFF;
    img_buffer[2] = (sec_limit >> 8) & 0xFF;
    img_buffer[3] = sec_limit & 0xFF;

    img_buffer[6] = 2;
}


void ScsiHardDisk::format() {
}

void ScsiHardDisk::read(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len) {
    uint32_t transfer_size = transfer_len;

    std::memset(img_buffer, 0, sizeof(img_buffer));

    if (cmd_len == 6)
        transfer_size = (transfer_len == 0) ? 256 : transfer_len;

    transfer_size *= sector_size;
    uint64_t device_offset = lba * sector_size;

    this->hdd_img.seekg(device_offset, this->hdd_img.beg);
    this->hdd_img.read(img_buffer, transfer_size);
}

void ScsiHardDisk::write(uint32_t lba, uint16_t transfer_len, uint8_t cmd_len) {
    uint32_t transfer_size = transfer_len;

    if (cmd_len == 6)
        transfer_size = (transfer_len == 0) ? 256 : transfer_len;

    transfer_size *= sector_size;
    uint64_t device_offset = lba * sector_size;

    this->hdd_img.seekg(device_offset, this->hdd_img.beg);
    this->hdd_img.write((char*)&img_buffer, transfer_size);
}

void ScsiHardDisk::seek(uint32_t lba) {
    uint64_t device_offset = lba * sector_size;
    this->hdd_img.seekg(device_offset, this->hdd_img.beg);
}

void ScsiHardDisk::rewind() {
    this->hdd_img.seekg(0, this->hdd_img.beg);
}

static const PropMap SCSI_HD_Properties = {
    {"hdd_img", new StrProperty("")},
    {"hdd_wr_prot", new BinProperty(0)},
};

static const DeviceDescription SCSI_HD_Descriptor = 
    {ScsiHardDisk::create, {}, SCSI_HD_Properties};

REGISTER_DEVICE(ScsiDevice, SCSI_HD_Descriptor);