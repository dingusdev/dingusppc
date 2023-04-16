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

#ifndef NVRAM_H
#define NVRAM_H

#include <devices/common/hwcomponent.h>

#include <cinttypes>
#include <memory>
#include <string>

/** @file Non-volatile RAM emulation.

    It implements a non-volatile random access storage whose content will be
    automatically saved to and restored from the dedicated file.
 */

class NVram : public HWComponent {
public:
    NVram(std::string file_name = "nvram.bin", uint32_t ram_size = 8192);
    ~NVram();

    static std::unique_ptr<HWComponent> create() {
        return std::unique_ptr<NVram>(new NVram());
    }

    uint8_t read_byte(uint32_t offset);
    void write_byte(uint32_t offset, uint8_t value);

private:
    std::string file_name; // file name for the backing file
    uint16_t    ram_size;  // NVRAM size
    std::unique_ptr<uint8_t[]>  storage;

    void init();
    void save();
};

#endif /* NVRAM_H */
