#include <iostream>
#include "addressmap.h"

AddressMap::AddressMap()
{
    this->address_map.clear();
}

AddressMap::~AddressMap()
{
    for (auto &entry : address_map) {
        if (entry.devobj)
            delete(entry.devobj);
    }
    this->address_map.clear();
}

void AddressMap::add(AddressMapEntry &entry)
{
    if (entry.type & RT_ROM) {
        printf("ROM entry added.\n");
        printf("entry.mem_ptr = %llx\n", (uint64_t)entry.mem_ptr);
    }

    address_map.push_back(entry);
}

AddressMapEntry *AddressMap::get_range(uint32_t addr)
{
    for (auto &entry : address_map) {
        if (addr >= entry.start && addr <= entry.end)
            return &entry;
    }

    return 0;
}
