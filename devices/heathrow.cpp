//DingusPPC
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 or powermax#2286 on Discord)

#include <cinttypes>
#include <iostream>
#include "macio.h"
#include "viacuda.h"

/** Heathrow Mac I/O device emulation.

    Author: Max Poliakovski 2019
*/

using namespace std;

HeathrowIC::HeathrowIC() : PCIDevice("mac-io/heathrow")
{
    this->viacuda = new ViaCuda();
    this->nvram   = new NVram();
}

HeathrowIC::~HeathrowIC()
{
    if (this->nvram)
        delete(this->nvram);

    if (this->viacuda)
        delete(this->viacuda);
}


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
            this->base_addr = value & 0xFFF80000;
            this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
            cout << this->name << " base address set to " << hex << this->base_addr
                << endl;
        }
        break;
    }
}

uint32_t HeathrowIC::read(uint32_t offset, int size)
{
    uint32_t res = 0;

    cout << this->name << ": reading from offset " << hex << offset << endl;

    unsigned sub_dev = (offset >> 12) & 0x3F;

    switch(sub_dev) {
    case 0:
        res = mio_ctrl_read(offset, size);
        break;
    case 8:
        cout << "DMA channel register space" << endl;
        break;
    case 0x14:
        cout << "AWACS-Screamer register space" << endl;
        break;
    case 0x16:
    case 0x17:
        res = this->viacuda->read((offset - 0x16000) >> 9);
        break;
    case 0x60:
    case 0x70:
        res = this->nvram->read_byte((offset - 0x60000) >> 4);
    default:
        cout << "unmapped I/O space: " << sub_dev << endl;
    }

    return res;
}

void HeathrowIC::write(uint32_t offset, uint32_t value, int size)
{
    cout << this->name << ": writing to offset " << hex << offset << endl;

    unsigned sub_dev = (offset >> 12) & 0x3F;

    switch(sub_dev) {
    case 0:
        mio_ctrl_write(offset, value, size);
        break;
    case 8:
        cout << "DMA channel register space" << endl;
        break;
    case 0x14:
        cout << "AWACS-Screamer register space" << endl;
        break;
    case 0x16:
    case 0x17:
        this->viacuda->write((offset - 0x16000) >> 9, value);
        break;
    default:
        cout << "unmapped I/O space: " << sub_dev << endl;
    }
}

uint32_t HeathrowIC::mio_ctrl_read(uint32_t offset, int size)
{
    uint32_t res = 0;

    switch(offset & 0xFF) {
    case 0x24:
        cout << "read from MIO:Int_Mask1 register" << endl;
        res = this->int_mask1;
        break;
    case 0x28:
        cout << "read from MIO:Int_Clear1 register" << endl;
        res = this->int_clear1;
        break;
    case 0x34: /* heathrowIDs / HEATHROW_MBCR (Linux): media bay config reg? */
        res = 0xF0700000UL;
        break;
    case 0x38:
        cout << "read from MIO:Feat_Ctrl register" << endl;
        res = this->feat_ctrl;
        break;
    default:
        cout << "unknown MIO register at " << hex << offset << endl;
        break;
    }

    return res;
}

void HeathrowIC::mio_ctrl_write(uint32_t offset, uint32_t value, int size)
{
    switch(offset & 0xFF) {
    case 0x24:
        cout << "write " << hex << value << " to MIO:Int_Mask1 register" << endl;
        this->int_mask1 = value;
        break;
    case 0x28:
        cout << "write " << hex << value << " to MIO:Int_Clear1 register" << endl;
        this->int_clear1 = value;
        break;
    case 0x38:
        cout << "write " << hex << value << " to MIO:Feat_Ctrl register" << endl;
        this->feat_ctrl = value;
        break;
    default:
        cout << "unknown MIO register at " << hex << offset << endl;
        break;
    }
}
