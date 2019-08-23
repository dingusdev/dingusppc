#include <cinttypes>
#include <iostream>
#include "macio.h"

using namespace std;

uint32_t HeathrowIC::pci_cfg_read(uint32_t reg_offs, uint32_t size)
{
    return this->pci_cfg_hdr[reg_offs & 0xFF];
}

void HeathrowIC::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size)
{
    switch(reg_offs) {
    case CFG_REG_BAR0: // base address register
        value =  LE2BE(value);
        if (value == 0xFFFFFFFF) {
            cout << this->name << " err: BAR0 block size determination not "
                 << "implemented yet" << endl;
        } else if (value & 1) {
            cout << this->name << " err: BAR0 I/O space not supported!" << endl;
        } else if (value & 0x06) {
            cout << this->name << " err: BAR0 64-bit I/O space not supported!"
                 << endl;
        } else {
            this->base_addr = value & 0xFFFFFFF0;
            this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
            cout << this->name << " base address set to " << hex << this->base_addr
                << endl;
        }
        break;
    }
}

uint32_t HeathrowIC::read(uint32_t offset, int size)
{
    cout << this->name << ": reading from offset " << hex << offset << endl;
    return 0;
}

void HeathrowIC::write(uint32_t offset, uint32_t value, int size)
{
    cout << this->name << ": writing to offset " << hex << offset << endl;
}
