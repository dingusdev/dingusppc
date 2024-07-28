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

#include <devices/memctrl/memctrlbase.h>
#include <devices/common/mmiodevice.h>

#include <algorithm>
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


static inline bool match_mem_entry(const AddressMapEntry* entry,
                                   const uint32_t start, const uint32_t end,
                                   MMIODevice* dev_instance)
{
    return start == entry->start && end == entry->end &&
        (!dev_instance || dev_instance == entry->devobj);
}


AddressMapEntry* MemCtrlBase::find_range(uint32_t addr) {
    for (auto& entry : address_map) {
        if (addr >= entry->start && addr <= entry->end)
            return entry;
    }

    return nullptr;
}


AddressMapEntry* MemCtrlBase::find_range_exact(uint32_t addr, uint32_t size,
                                               MMIODevice* dev_instance)
{
    if (size) {
        const uint32_t end = addr + size - 1;
        for (auto& entry : address_map) {
            if (match_mem_entry(entry, addr, end, dev_instance))
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
                LOG_F(WARNING, "memory region 0x%X..0x%X%s%s%s already exists",
                    addr, end,
                    entry->devobj ? " (" : "",
                        entry->devobj ? entry->devobj->get_name().c_str() : "",
                        entry->devobj ? ")"
                        : ""
                );
                result = false;
            }
            else if (addr >= entry->start && end <= entry->end) {
                LOG_F(WARNING, "0x%X..0x%X already exists in memory region 0x%X..0x%X%s%s%s",
                    addr, end, entry->start, entry->end,
                    entry->devobj ? " (" : "",
                        entry->devobj ? entry->devobj->get_name().c_str() : "",
                        entry->devobj ? ")"
                        : ""
                );
                result = false;
            }
            else if (end >= entry->start && addr <= entry->end) {
                LOG_F(ERROR, "0x%X..0x%X overlaps existing memory region 0x%X..0x%X%s%s%s",
                    addr, end, entry->start, entry->end,
                    entry->devobj ? " (" : "",
                        entry->devobj ? entry->devobj->get_name().c_str() : "",
                        entry->devobj ? ")"
                        : ""
                );
                result = false;
            }
        }
    }
    return result;
}


bool MemCtrlBase::add_mem_region(uint32_t start_addr, uint32_t size,
                                 uint32_t dest_addr, uint32_t type,
                                 uint8_t init_val = 0)
{
    AddressMapEntry *entry;

    // bail out if a memory region for the given range already exists
    if (!is_range_free(start_addr, size))
        return false;

    uint8_t* reg_content = new uint8_t[size](); // allocate and clear to zero

    this->mem_regions.push_back(reg_content);

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + size - 1;
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = dest_addr;
    entry->type    = type;
    entry->devobj  = nullptr;
    entry->mem_ptr = reg_content;

    // Keep address_map sorted, that way the RAM region (which starts at 0 and
    // is most often requested) will be found by find_range on the first
    // iteration.
    this->address_map.insert(
        std::upper_bound(
            this->address_map.begin(),
            this->address_map.end(),
            entry,
            [](const auto& lhs, const auto& rhs) {
                return lhs->start < rhs->start;
            }),
            entry);

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


bool MemCtrlBase::add_mem_mirror_common(uint32_t start_addr, uint32_t dest_addr,
                                        uint32_t offset, uint32_t size) {
    AddressMapEntry *entry, *ref_entry;

    ref_entry = find_range(dest_addr);
    if (!ref_entry)
        return false;

    // use origin's size if no size was specified
    if (!size)
        size = ref_entry->end - ref_entry->start + 1;

    if (ref_entry->start + offset + size - 1 > ref_entry->end) {
        LOG_F(ERROR, "Partial mirror outside the origin, offset=0x%X, size=0x%X",
              offset, size);
        return false;
    }

    entry = new AddressMapEntry;

    uint32_t end   = start_addr + size - 1;
    entry->start   = start_addr;
    entry->end     = end;
    entry->mirror  = dest_addr;
    entry->type    = ref_entry->type | RT_MIRROR;
    entry->devobj  = nullptr;
    entry->mem_ptr = ref_entry->mem_ptr + offset;

    this->address_map.push_back(entry);

    LOG_F(INFO, "Added mem region mirror 0x%X..0x%X (%s%s%s%s) -> 0x%X : 0x%X..0x%X%s%s%s",
        start_addr, end,
        entry->type & RT_ROM ? "ROM," : "",
        entry->type & RT_RAM ? "RAM," : "",
        entry->type & RT_MMIO ? "MMIO," : "",
        entry->type & RT_MIRROR ? "MIRROR," : "",
        dest_addr,
        ref_entry->start + offset, ref_entry->end,
        ref_entry->devobj ? " (" : "",
            ref_entry->devobj ? ref_entry->devobj->get_name().c_str() : "",
            ref_entry->devobj ? ")"
        : ""
    );

    return true;
}


bool MemCtrlBase::add_mem_mirror(uint32_t start_addr, uint32_t dest_addr) {
    return this->add_mem_mirror_common(start_addr, dest_addr);
}


bool MemCtrlBase::add_mem_mirror_partial(uint32_t start_addr, uint32_t dest_addr,
                                         uint32_t offset, uint32_t size) {
    return this->add_mem_mirror_common(start_addr, dest_addr, offset, size);
}


bool MemCtrlBase::set_data(uint32_t load_addr, const uint8_t* data, uint32_t size) {
    AddressMapEntry* ref_entry;
    uint32_t cpy_size;

    ref_entry = find_range(load_addr);
    if (!ref_entry)
        return false;

    uint32_t load_offset = load_addr - ref_entry->start;

    cpy_size = std::min(ref_entry->end - ref_entry->start + 1, size);
    memcpy(ref_entry->mem_ptr + load_offset, data, cpy_size);

    return true;
}


bool MemCtrlBase::add_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice* dev_instance)
{
    AddressMapEntry *entry;

    // bail out if a memory region for the given range already exists
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

    LOG_F(INFO, "Added mmio region 0x%X..0x%X%s%s%s",
        start_addr, end,
        dev_instance ? " (" : "",
            dev_instance ? dev_instance->get_name().c_str() : "",
            dev_instance ? ")"
            : ""
    );

    return true;
}

bool MemCtrlBase::remove_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice* dev_instance)
{
    int found = 0;

    uint32_t end = start_addr + size - 1;
    address_map.erase(std::remove_if(address_map.begin(), address_map.end(),
        [start_addr, end, dev_instance, &found](const AddressMapEntry *entry) {
            bool result = match_mem_entry(entry, start_addr, end, dev_instance);
            found += result;
            return result;
        }
    ), address_map.end());

    if (found == 0)
        LOG_F(ERROR, "Cannot find mmio region 0x%X..0x%X%s%s%s to remove",
            start_addr, end,
            dev_instance ? " (" : "",
                dev_instance ? dev_instance->get_name().c_str() : "",
                dev_instance ? ")"
                : ""
        );
    else if (found > 1)
        LOG_F(ERROR, "Removed %d occurrences of mmio region 0x%X..0x%X%s%s%s",
            found, start_addr, end,
            dev_instance ? " (" : "",
            dev_instance ? dev_instance->get_name().c_str() : "",
            dev_instance ? ")"
            : ""
        );
    else
        LOG_F(INFO, "Removed mmio region 0x%X..0x%X%s%s%s",
            start_addr, end,
            dev_instance ? " (" : "",
                dev_instance ? dev_instance->get_name().c_str() : "",
                dev_instance ? ")"
                : ""
        );

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

uint8_t *MemCtrlBase::get_region_hostmem_ptr(const uint32_t addr) {
    AddressMapEntry *reg_desc = this->find_range(addr);
    if (reg_desc == nullptr || reg_desc->type == RT_MMIO)
        return nullptr;

    if (reg_desc->type == RT_MIRROR)
        return (addr - reg_desc->mirror) + reg_desc->mem_ptr;
    else
        return (addr - reg_desc->start) + reg_desc->mem_ptr;
}
