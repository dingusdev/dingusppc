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

/** @file SCSI hard drive support */

#ifndef SCSI_HD_H
#define SCSI_HD_H

#include <devices/common/scsi/scsi.h>
#include <cinttypes>
#include <fstream>
#include <stdio.h>
#include <string>

class SCSI_HD : public ScsiDevice {
public:
    SCSI_HD();
    ~SCSI_HD() = default;

    virtual void notify(ScsiMsg msg_type, int param) = 0;

    int test_unit_ready();
    int req_sense(uint8_t alloc_len);
    int read_capacity_10();
    int inquiry();
    int send_diagnostic();

    void format();
    void read(uint32_t lba, uint16_t transfer_len);
    void write(uint32_t lba, uint16_t transfer_len);
    void seek(uint32_t lba);
    void rewind();

protected:
    std::string img_path;
    std::fstream hdd_img;
    uint64_t img_size;
    char img_buffer[1 << 17];
    uint8_t scsi_command[12];
    uint64_t file_offset = 0;
    uint8_t status;
};

#endif