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

#ifndef OF_NVRAM_H
#define OF_NVRAM_H

#include <devices/common/nvram.h>

/** @file Utilities for working with the Apple OpenFirmware NVRAM partition. */

#define OF_NVRAM_OFFSET 0x1800
#define OF_NVRAM_SIG    0x1275

// OF Variable types
enum {
    OF_VAR_TYPE_INT = 1,
    OF_VAR_TYPE_STR = 2,
};

typedef struct {
    uint16_t    sig;        // partition signature (= 0x1275)
    uint8_t     version;    // header version
    uint8_t     num_pages;  // number of memory pages
    uint16_t    checksum;   // partition checksum
    uint16_t    here;       // offset to the next free byte (bottom-up)
    uint16_t    top;        // offset to the last free byte (top-down)
} OfNvramHdr;

class OfNvramUtils {
public:
    OfNvramUtils()  = default;
    ~OfNvramUtils() = default;

    int  init();
    void printenv();
    void setenv(std::string var_name, std::string value);

protected:
    bool validate();
    uint16_t checksum_partition();
    void update_partition();

private:
    NVram*  nvram_obj = nullptr;
    int     size = 0;
    uint8_t buf[0x800];
};

#endif // OF_NVRAM_H
