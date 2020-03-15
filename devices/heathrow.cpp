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

#include <thirdparty/loguru.hpp>
#include <cinttypes>
#include <iostream>
#include "macio.h"
#include "viacuda.h"
#include "awacs.h"
#include "machines/machinebase.h"

/** Heathrow Mac I/O device emulation.

    Author: Max Poliakovski 2019
*/

using namespace std;

HeathrowIC::HeathrowIC() : PCIDevice("mac-io/heathrow")
{
    this->nvram    = new NVram();

    this->viacuda  = new ViaCuda();
    gMachineObj->add_subdevice("ViaCuda", this->viacuda);

    this->screamer = new AWACDevice();
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
    switch (reg_offs) {
    case CFG_REG_BAR0: // base address register
        value = LE2BE(value);
        if (value == 0xFFFFFFFF) {
            LOG_F(ERROR, "%s err: BAR0 block size determination not \
                implemented yet \n", this->name.c_str());
        }
        else if (value & 1) {
            LOG_F(ERROR, "%s err: BAR0 I/O space not supported! \n", this->name.c_str());
        }
        else if (value & 0x06) {
            LOG_F(ERROR, "%s err: BAR0 64-bit I/O space not supported! \n", this->name.c_str());
        }
        else {
            this->base_addr = value & 0xFFF80000;
            this->host_instance->pci_register_mmio_region(this->base_addr, 0x80000, this);
            LOG_F(INFO, "%s base address set to %x \n", this->name.c_str(), this->base_addr);
        }
        break;
    }
}

uint32_t HeathrowIC::read(uint32_t offset, int size)
{
    uint32_t res = 0;

    LOG_F(9, "%s: reading from offset %x \n", this->name.c_str(), offset);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        res = mio_ctrl_read(offset, size);
        break;
    case 8:
        LOG_F(WARNING, "Attempting to read DMA channel register space \n");
        break;
    case 0x14:
        res = this->screamer->snd_ctrl_read(offset - 0x14000, size);
        break;
    case 0x16:
    case 0x17:
        res = this->viacuda->read((offset - 0x16000) >> 9);
        break;
    default:
        if (sub_addr >= 0x60) {
            res = this->nvram->read_byte((offset - 0x60000) >> 4);
        }
        else {
            LOG_F(WARNING, "Attempting to read unmapped I/O space: %x \n", offset);
        }
    }

    return res;
}

void HeathrowIC::write(uint32_t offset, uint32_t value, int size)
{
    LOG_F(9, "%s: writing to offset %x \n", this->name.c_str(), offset);

    unsigned sub_addr = (offset >> 12) & 0x7F;

    switch (sub_addr) {
    case 0:
        mio_ctrl_write(offset, value, size);
        break;
    case 8:
        LOG_F(WARNING, "Attempting to write to DMA channel register space \n");
        break;
    case 0x14:
        this->screamer->snd_ctrl_write(offset - 0x14000, value, size);
        break;
    case 0x16:
    case 0x17:
        this->viacuda->write((offset - 0x16000) >> 9, value);
        break;
    default:
        if (sub_addr >= 0x60) {
            this->nvram->write_byte((offset - 0x60000) >> 4, value);
        }
        else {
            LOG_F(WARNING, "Attempting to write to unmapped I/O space: %x \n", offset);
        }
    }
}

uint32_t HeathrowIC::mio_ctrl_read(uint32_t offset, int size)
{
    uint32_t res = 0;

    switch (offset & 0xFF) {
    case 0x24:
        LOG_F(9, "read from MIO:Int_Mask1 register \n");
        res = this->int_mask1;
        break;
    case 0x28:
        LOG_F(9, "read from MIO:Int_Clear1 register \n");
        res = this->int_clear1;
        break;
    case 0x34: /* heathrowIDs / HEATHROW_MBCR (Linux): media bay config reg? */
        res = 0xF0700000UL;
        break;
    case 0x38:
        LOG_F(9, "read from MIO:Feat_Ctrl register \n");
        res = this->feat_ctrl;
        break;
    default:
        LOG_F(WARNING, "unknown MIO register at %x \n", offset);
        break;
    }

    return res;
}

void HeathrowIC::mio_ctrl_write(uint32_t offset, uint32_t value, int size)
{
    switch (offset & 0xFF) {
    case 0x24:
        LOG_F(9, "write %x to MIO:Int_Mask1 register \n", value);
        this->int_mask1 = value;
        break;
    case 0x28:
        LOG_F(9, "write %x to MIO:Int_Clear1 register \n", value);
        this->int_clear1 = value;
        break;
    case 0x38:
        LOG_F(9, "write %x to MIO:Feat_Ctrl register \n", value);
        this->feat_ctrl = value;
        break;
    default:
        LOG_F(WARNING, "unknown MIO register at %x \n", offset);
        break;
    }
}
