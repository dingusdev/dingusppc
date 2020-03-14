/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

#ifndef MEMORY_CONTROLLER_BASE_H
#define MEMORY_CONTROLLER_BASE_H

#include <cinttypes>
#include <string>
#include <vector>
#include "mmiodevice.h"

enum RangeType {
    RT_ROM    = 1,  /* read-only memory */
    RT_RAM    = 2,  /* random access memory */
    RT_MMIO   = 4,  /* memory mapped I/O */
    RT_MIRROR = 8   /* region mirror (content of another region is acessible
                       at some other address) */
};

typedef struct AddressMapEntry {
    uint32_t        start;   /* first address of the corresponding range */
    uint32_t        end;     /* last  address of the corresponding range */
    uint32_t        mirror;  /* mirror address for RT_MIRROR */
    uint32_t        type;    /* range type */
    MMIODevice     *devobj;  /* pointer to device object */
    unsigned char  *mem_ptr; /* direct pointer to data for memory objects */
} AddressMapEntry;


/** Base class for memory controllers. */
class MemCtrlBase {
public:
    MemCtrlBase() = default;
    virtual ~MemCtrlBase();
    virtual bool add_rom_region(uint32_t start_addr, uint32_t size);
    virtual bool add_ram_region(uint32_t start_addr, uint32_t size);
    virtual bool add_mem_mirror(uint32_t start_addr, uint32_t dest_addr);

    virtual bool add_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice *dev_instance);

    virtual bool set_data(uint32_t reg_addr, const uint8_t *data, uint32_t size);

    AddressMapEntry *find_range(uint32_t addr);

protected:
    bool add_mem_region(uint32_t start_addr, uint32_t size, uint32_t dest_addr,
        uint32_t type, uint8_t init_val);

private:
    std::vector<uint8_t *> mem_regions;
    std::vector<AddressMapEntry> address_map;
};

#endif /* MEMORY_CONTROLLER_BASE_H */
