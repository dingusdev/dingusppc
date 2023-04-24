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

/** @file ATA hard disk definitions. */

#ifndef ATA_HARD_DISK_H
#define ATA_HARD_DISK_H

#include <devices/common/ata/atabasedevice.h>
#include <string>
#include <fstream>

#define ATA_HD_SEC_SIZE 512

class AtaHardDisk : public AtaBaseDevice
{
public:
    AtaHardDisk();
    ~AtaHardDisk() = default;

    void insert_image(std::string filename);
    int perform_command() override;

private:
    std::fstream hdd_img;
    uint64_t img_size;
    char * buffer = new char[1 <<17];

    uint8_t hd_id_data[ATA_HD_SEC_SIZE] = {};
};

#endif // ATA_HARD_DISK_H
