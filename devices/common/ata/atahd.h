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

/** @file ATA hard disk definitions. */

#ifndef ATA_HARD_DISK_H
#define ATA_HARD_DISK_H

#include <devices/common/ata/atabasedevice.h>
#include <utils/imgfile.h>

#include <string>

#define ATA_HD_SEC_SIZE 512
#define SECTORS_PER_INT 16

// C:16383 x H:16 x S:63 = C:1032 x H:254 x S:63 = 8063.5078125 MiB = 8.46 GB
#define ATA_BIOS_LIMIT  16514064
// C:65535 x H:16 x S:255 = 127.498 GiB = 136.900 GB = largest identify
#define REAL_CHS_LIMIT  267382800
// C:65536 x H:16 x S:255 = 127.500 GiB = 136.902 GB = largest address
#define CHS_LIMIT       267386880

class AtaHardDisk : public AtaBaseDevice
{
public:
    AtaHardDisk(std::string name);
    ~AtaHardDisk() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AtaHardDisk>(new AtaHardDisk("ATA-HD"));
    }

    int device_postinit() override;

    void insert_image(std::string filename);
    int perform_command() override;

protected:
    void        prepare_identify_info();
    uint64_t    get_lba();
    void        calc_chs_params();

private:
    ImgFile     hdd_img;
    uint64_t    img_size = 0;
    uint32_t    total_sectors = 0;
    uint64_t    cur_fpos = 0;

    // fictive disk geometry for CHS-to-LBA translation
    uint16_t    cylinders;
    uint8_t     heads;
    uint8_t     sectors;

    uint8_t     sectors_per_int  = 0; // sectors per interrupt for READ_MULTIPLE/WRITE_MULTIPLE

    char * buffer = new char[1 <<17];
};

#endif // ATA_HARD_DISK_H
