#include <string>
#include <vector>
#include <algorithm> // to shut up MSVC errors (:

#include "memctrlbase.h"


MemCtrlBase::MemCtrlBase(std::string name)
{
    this->name = name;
}


MemCtrlBase::~MemCtrlBase()
{
    for (auto &reg : mem_regions) {
        if (reg)
            delete(reg);
    }
    this->mem_regions.clear();
    this->address_map.clear();
}


AddressMapEntry *MemCtrlBase::find_range(uint32_t addr)
{
    for (auto &entry : address_map) {
        if (addr >= entry.start && addr <= entry.end)
            return &entry;
    }

    return 0;
}


bool MemCtrlBase::add_mem_region(uint32_t start_addr, uint32_t size,
            uint32_t dest_addr, uint32_t type, uint8_t init_val = 0)
{
    AddressMapEntry entry;

    /* error if a memory region for the given range already exists */
    if (find_range(start_addr) || find_range(start_addr + size))
        return false;

    uint8_t *reg_content = new uint8_t[size];
    if (!reg_content)
        return false;

    this->mem_regions.push_back(reg_content);

    entry.start   = start_addr;
    entry.end     = start_addr + size - 1;
    entry.mirror  = dest_addr;
    entry.type    = type;
    entry.devobj  = 0;
    entry.mem_ptr = reg_content;

    this->address_map.push_back(entry);

    return true;
}


bool MemCtrlBase::add_rom_region(uint32_t start_addr, uint32_t size)
{
    return add_mem_region(start_addr, size, 0, RT_ROM);
}


bool MemCtrlBase::add_ram_region(uint32_t start_addr, uint32_t size)
{
    return add_mem_region(start_addr, size, 0, RT_RAM);
}


bool MemCtrlBase::add_mem_mirror(uint32_t start_addr, uint32_t dest_addr)
{
    AddressMapEntry entry, *ref_entry;

    ref_entry = find_range(dest_addr);
    if (!ref_entry)
        return false;

    entry.start   = start_addr;
    entry.end     = start_addr + (ref_entry->end - ref_entry->start) + 1;
    entry.mirror  = dest_addr;
    entry.type    = ref_entry->type | RT_MIRROR;
    entry.devobj  = 0;
    entry.mem_ptr = ref_entry->mem_ptr;

    this->address_map.push_back(entry);

    return true;
}


bool MemCtrlBase::set_data(uint32_t reg_addr, const uint8_t *data, uint32_t size)
{
    AddressMapEntry *ref_entry;
    uint32_t cpy_size;

    ref_entry = find_range(reg_addr);
    if (!ref_entry)
        return false;

    cpy_size = std::min(ref_entry->end - ref_entry->start + 1, size);
    memcpy(ref_entry->mem_ptr, data, cpy_size);

    return true;
}


bool MemCtrlBase::add_mmio_region(uint32_t start_addr, uint32_t size,
                    MMIODevice *dev_instance)
{
    AddressMapEntry entry;

    /* error if another region for the given range already exists */
    if (find_range(start_addr) || find_range(start_addr + size))
        return false;

    entry.start   = start_addr;
    entry.end     = start_addr + size - 1;
    entry.mirror  = 0;
    entry.type    = RT_MMIO;
    entry.devobj  = dev_instance;
    entry.mem_ptr = 0;

    this->address_map.push_back(entry);

    return true;
}
