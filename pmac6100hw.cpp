#include <iostream>
#include "ppcemumain.h"
#include "pmac6100hw.h"
#include "addressmap.h"


/** ======== High-speed memory controller (HMC) implementation ======== */
HMC::HMC()
{
    this->config_reg = 0ULL;
    this->bit_pos = 0;
}

uint32_t HMC::read(uint32_t offset, int size)
{
    if (!offset)
        return !!(this->config_reg & (1ULL << this->bit_pos++));
    else
        return 0; /* FIXME: what should be returned for invalid offsets? */
}

void HMC::write(uint32_t offset, uint32_t value, int size)
{
    uint64_t bit;

    switch(offset) {
        case 0:
            bit = 1ULL << this->bit_pos++;
            this->config_reg = (value & 1) ? this->config_reg | bit :
                               this->config_reg & ~bit;
            break;
        case 8: /* writing to HMCBase + 8 resets internal bit position */
            this->bit_pos = 0;
            break;
    }
}


/** Initialize Power Macintosh 6100 hardware. */
void pmac6100_init()
{
    AddressMapEntry entry;

    machine_phys_map = new AddressMap();

    /* add ROM and its mirror */
    entry = {0xFFC00000, 0xFFFFFFFF, 0, RT_ROM, 0, machine_sysrom_mem};
    machine_phys_map->add(entry);

    entry = {0x40000000, 0x403FFFFF, 0, RT_MIRROR | RT_ROM, 0, machine_sysrom_mem};
    machine_phys_map->add(entry);

    /* add soldered on-board RAM. FIXME: do it properly using memory objects */
    entry = {0x00000000, 0x007FFFFF, 0, RT_RAM, 0, machine_sysram_mem};
    machine_phys_map->add(entry);

    /* this machine has two SIMM slots, each of them can hold max. 128 MB RAM */
    /* SIMM slot 2 physical range: 0x08000000 - 0x0FFFFFFF */
    /* SIMM slot 1 physical range: 0x10000000 - 0x17FFFFFF */
    /* TODO: make those SIMM slots end-user configurable */

    /* add high-speed memory controller I/O space */
    entry = {0x50F40000, 0x50F4FFFF, 0, RT_MMIO, new HMC()};
    machine_phys_map->add(entry);

    /* add read-only machine identification register */
    entry = {0x5FFFFFFC, 0x5FFFFFFF, 0, RT_MMIO, new CPUID(PM6100)};
    machine_phys_map->add(entry);

    printf("Machine initialization completed.\n");
}
