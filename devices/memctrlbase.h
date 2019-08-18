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
    MemCtrlBase(std::string name);
    virtual ~MemCtrlBase();
    virtual bool add_rom_region(uint32_t start_addr, uint32_t size);
    virtual bool add_ram_region(uint32_t start_addr, uint32_t size);
    virtual bool add_mem_mirror(uint32_t start_addr, uint32_t dest_addr);

    virtual bool add_mmio_region(uint32_t start_addr, uint32_t size, MMIODevice *dev_instance);

    virtual bool set_data(uint32_t reg_addr, const uint8_t *data, uint32_t size);

    AddressMapEntry *find_range(uint32_t addr);

protected:
    bool add_mem_region(uint32_t start_addr, uint32_t size, uint32_t dest_addr, uint32_t type, uint8_t init_val);

    std::string name;

private:
    std::vector<uint8_t *> mem_regions;
    std::vector<AddressMapEntry> address_map;
};

#endif /* MEMORY_CONTROLLER_BASE_H */
