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

/** @file Full ATA device */

#include <devices/common/ata/ata_bus.h>

#define SEC_SIZE 512

#ifndef ATA_FULL_H
#define ATA_FULL_H

class AtaFullDevice : public AtaBus {
    public:
    AtaFullDevice();
    ~AtaFullDevice() = default;

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<AtaFullDevice>(new AtaFullDevice());
    }

    void insert_image(std::string filename);
    uint32_t read(int reg);
    void write(int reg, uint32_t value);

    int perform_command(uint32_t command);
    void get_status();

    private:
    uint32_t regs[33] = {0x0};
    uint8_t buffer[SEC_SIZE];
    std::fstream ide_img;
    uint64_t img_size;
};


#endif