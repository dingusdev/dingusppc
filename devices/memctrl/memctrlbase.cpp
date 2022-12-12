/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

#include <devices/memctrl/memctrlbase.h>

#include <algorithm>    // to shut up MSVC errors (:
#include <cstring>
#include <string>
#include <vector>
#include <loguru.hpp>


MemCtrlBase::~MemCtrlBase() {
    for (auto& entry : address_map) {
        if (entry)
            delete(entry);
    }

    for (auto& reg : mem_regions) {
        if (reg)
            delete (reg);
    }
    this->mem_regions.clear();
    this->address_map.clear();
}


AddressMapEntry* MemCtrlBase::find_range(uint32_t addr) {
    for (auto& entry : address_map) {
        if (addr >= entry->start && addr <= entry->end)
            return entry;
    }

    return nullptr;
}


AddressMapEntry* MemCtrlBase::find_range_exact(uint32_t addr, uint32_t size, MMIODevice* dev_instance) {
    if (size) {
        uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (addr == entry->start && end == entry->end && (!dev_instance || dev_instance == entry->devobj) )
                return entry;
        }
    }

    return nullptr;
}


AddressMapEntry* MemCtrlBase::find_range_contains(uint32_t addr, uint32_t size) {
    if (size) {
        uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (addr >= entry->start && end <= entry->end)
                return entry;
        }
    }

    return nullptr;
}


AddressMapEntry* MemCtrlBase::find_range_overlaps(uint32_t addr, uint32_t size) {
    if (size) {
        uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (end >= entry->start && addr <= entry->end)
                return entry;
        }
    }

    return nullptr;
}


bool MemCtrlBase::is_range_free(uint32_t addr, uint32_t size) {
    bool result = true;
    if (size) {
        uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (addr == entry->start && end == entry->end) {
                LOG_F(WARNING, "memory region 0x%X..0x%X%s%s%s already exists", addr, end, entry->devobj ? " (" : "", entry->devobj ? entry->devobj->get_name().c_str() : "", entry->devobj ? ")" : "");
                result = false;
            }
            else if (addr >= entry->start && end <= entry->end) {
                LOG_F(WARNING, "0x%X..0x%X already exists in memory region 0x%X..0x%X%s%s%s", addr, end, entry->start, entry->end, entry->devobj ? " (" : "", entry->devobj ? entry->devobj->get_name().c_str() : "", entry->devobj ? ")" : "");
                result = false;
            }
            else if (end >= entry->start && addr <= entry->end) {
                LOG_F(ERROR, "0x%X..0x%X overlaps existing memory region 0x%X..0x%X%s%s%s", addr, end, entry->start, entry->end, entry->devobj ? " (" : "", entry->devobj ? entry->devobj->get_name().c_str() : "", entry->devobj ? ")" : "");
                result = false;
            }
        }
    }
    return result;
}


bool MemCtrlBase::add_mem_region(
    uint32_t start_addr, uint32_t size, uint32_t dest_addr, uint32_t type, uint8_t init_val = 0) {
    AddressMapEntry *entry;

    /* error if a memory region for the given range already exists */
    if (!is_range_free(start_addr, size))
        return false;

    uint8_t* reg_content = new uint8_t[size];

    this->mem_regions.push_back(reg_content);

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + size - 1;
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = dest_addr;
    entry->type    = type;
    entry->devobj  = 0;
    entry->mem_ptr = reg_content;

    this->address_map.push_back(entry);

    LOG_F(INFO, "Added mem region 0x%X..0x%X (%s%s%s%s) -> 0x%X", start_addr, end,
        entry->type & RT_ROM ? "ROM," : "",
        entry->type & RT_RAM ? "RAM," : "",
        entry->type & RT_MMIO ? "MMIO," : "",
        entry->type & RT_MIRROR ? "MIRROR," : "",
        dest_addr
    );

    return true;
}


bool MemCtrlBase::add_rom_region(uint32_t start_addr, uint32_t size) {
    return add_mem_region(start_addr, size, 0, RT_ROM);
}


bool MemCtrlBase::add_ram_region(uint32_t start_addr, uint32_t size) {
    return add_mem_region(start_addr, size, 0, RT_RAM);
}


bool MemCtrlBase::add_mem_mirror(uint32_t start_addr, uint32_t dest_addr) {
    AddressMapEntry *entry, *ref_entry;

    ref_entry = find_range(dest_addr);
    if (!ref_entry)
        return false;

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + (ref_entry->end - ref_entry->start);
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = dest_addr;
    entry->type    = ref_entry->type | RT_MIRROR;
    entry->devobj  = 0;
    entry->mem_ptr = ref_entry->mem_ptr;

    this->address_map.push_back(entry);

    LOG_F(INFO, "Added mem region mirror 0x%X..0x%X (%s%s%s%s) -> 0x%X : 0x%X..0x%X%s%s%s", start_addr, end,
        entry->type & RT_ROM ? "ROM," : "",
        entry->type & RT_RAM ? "RAM," : "",
        entry->type & RT_MMIO ? "MMIO," : "",
        entry->type & RT_MIRROR ? "MIRROR," : "",
        dest_addr,
        ref_entry->start, ref_entry->end,
        ref_entry->devobj ? " (" : "", ref_entry->devobj ? ref_entry->devobj->get_name().c_str() : "", ref_entry->devobj ? ")" : ""
    );

    return true;
}


bool MemCtrlBase::set_data(uint32_t reg_addr, const uint8_t* data, uint32_t size) {
    AddressMapEntry* ref_entry;
    uint32_t cpy_size;

    ref_entry = find_range(reg_addr);
    if (!ref_entry)
        return false;

    cpy_size = std::min(ref_entry->end - ref_entry->start + 1, size);
    memcpy(ref_entry->mem_ptr, data, cpy_size);

    return true;
}


bool MemCtrlBase::add_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice* dev_instance) {
    AddressMapEntry *entry;

    /* error if a memory region for the given range already exists */
    if (!is_range_free(start_addr, size))
        return false;

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + size - 1;
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = 0;
    entry->type    = RT_MMIO;
    entry->devobj  = dev_instance;
    entry->mem_ptr = 0;

    this->address_map.push_back(entry);

    LOG_F(INFO, "Added mmio region 0x%X..0x%X%s%s%s", start_addr, end, dev_instance ? " (" : "", dev_instance ? dev_instance->get_name().c_str() : "", dev_instance ? ")" : "");

    return true;
}

bool MemCtrlBase::remove_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice* dev_instance) {
    int found = 0;

    uint32_t end = start_addr + size - 1;
    address_map.erase(std::remove_if(address_map.begin(), address_map.end(),
        [start_addr, end, dev_instance, &found](const AddressMapEntry *entry) {
            bool result = (start_addr == entry->start && end == entry->end && (!dev_instance || dev_instance == entry->devobj));
            found += result;
            return result;
        }
    ), address_map.end());

    if (found == 0)
        LOG_F(ERROR, "Cannot find mmio region 0x%X..0x%X%s%s%s to remove", start_addr, end, dev_instance ? " (" : "", dev_instance ? dev_instance->get_name().c_str() : "", dev_instance ? ")" : "");
    else if (found > 1)
        LOG_F(ERROR, "Removed %d occurrences of mmio region 0x%X..0x%X%s%s%s", found, start_addr, end, dev_instance ? " (" : "", dev_instance ? dev_instance->get_name().c_str() : "", dev_instance ? ")" : "");
    else
        LOG_F(INFO, "Removed mmio region 0x%X..0x%X%s%s%s", start_addr, end, dev_instance ? " (" : "", dev_instance ? dev_instance->get_name().c_str() : "", dev_instance ? ")" : "");
    
    return (found > 0);
}

AddressMapEntry* MemCtrlBase::find_rom_region()
{
    for (auto& entry : address_map) {
        if (entry->type == RT_ROM) {
            return entry;
        }
    }

    return nullptr;
}
