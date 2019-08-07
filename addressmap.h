#ifndef ADDRESS_MAP_H
#define ADDRESS_MAP_H

#include <cinttypes>
#include <vector>
#include "mmiodevice.h"

typedef uint32_t (*DevRead) (uint32_t offset, int size);
typedef void     (*DevWrite)(uint32_t offset, uint32_t value, int size);

enum RangeType {
    RT_ROM    = 1,
    RT_RAM    = 2,
    RT_MMIO   = 4,    /* memory mapped I/O */
    RT_MIRROR = 8
};

typedef struct AddressMapEntry {
    uint32_t        start;   /* first address of the corresponding range */
    uint32_t        end;     /* last  address of the corresponding range */
    uint32_t        mirror;  /* mirror address for RT_MIRROR */
    uint32_t        type;    /* range type */
    MMIODevice     *devobj;  /* pointer to device object */
    unsigned char  *mem_ptr; /* direct pointer to data for memory objects */
} AddressMapEntry;


class AddressMap {
public:
    AddressMap();
    ~AddressMap();
    void add(AddressMapEntry &entry);
    AddressMapEntry *get_range(uint32_t addr);

private:
    std::vector<AddressMapEntry> address_map;
};

#endif /* ADDRESS_MAP_H_ */
