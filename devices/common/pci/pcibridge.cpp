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

#include <devices/common/pci/pcibridge.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <loguru.hpp>

PCIBridge::PCIBridge(std::string name) : PCIBridgeBase(name, PCI_HEADER_TYPE_1, 2)
{
    this->pci_rd_memory_base        = [this]() { return this->memory_base; };
    this->pci_rd_memory_limit       = [this]() { return this->memory_limit; };
    this->pci_rd_io_base            = [this]() { return this->io_base; };
    this->pci_rd_io_limit           = [this]() { return this->io_limit; };
    this->pci_rd_pref_mem_base      = [this]() { return this->pref_mem_base; };
    this->pci_rd_pref_mem_limit     = [this]() { return this->pref_mem_limit; };
    this->pci_rd_io_base_upper16    = [this]() { return this->io_base_upper16; };
    this->pci_rd_io_limit_upper16   = [this]() { return this->io_limit_upper16; };
    this->pci_rd_pref_base_upper32  = [this]() { return this->pref_base_upper32; };
    this->pci_rd_pref_limit_upper32 = [this]() { return this->pref_limit_upper32; };

    this->pci_wr_memory_base = [this](uint16_t val) {
        this->memory_base    = (val & this->memory_cfg) | (this->memory_cfg & 15);
        this->memory_base_32 = ((this->memory_base & 0xfff0) << 16);
    };

    this->pci_wr_memory_limit = [this](uint16_t val) {
        this->memory_limit    = (val & this->memory_cfg) | (this->memory_cfg & 15);
        this->memory_limit_32 = ((this->memory_limit & 0xfff0) << 16) + 0x100000;
    };

    this->pci_wr_io_base = [this](uint8_t  val) {
        this->io_base    = (val & this->io_cfg) | (this->io_cfg & 15);
        this->io_base_32 = ((uint32_t)this->io_base_upper16 << 16) | ((this->io_base & 0xf0) << 8);
    };

    this->pci_wr_io_limit = [this](uint8_t  val) {
        this->io_limit    = (val & this->io_cfg) | (this->io_cfg & 15);
        this->io_limit_32 = (((uint32_t)this->io_limit_upper16 << 16) |
                            ((this->io_limit & 0xf0) << 8)) + 0x1000;
    };

    this->pci_wr_pref_mem_base = [this](uint16_t val) {
        this->pref_mem_base = (val & this->pref_mem_cfg) | (this->pref_mem_cfg & 15);
        this->pref_mem_base_64  = ((uint64_t)this->pref_base_upper32 << 32) |
                                  ((this->pref_mem_base & 0xfff0) << 16);
    };
    this->pci_wr_pref_mem_limit = [this](uint16_t val) {
        this->pref_mem_limit = (val & this->pref_mem_cfg) | (this->pref_mem_cfg & 15);
        this->pref_mem_limit_64 = (((uint64_t)this->pref_limit_upper32 << 32) |
                                   ((this->pref_mem_limit & 0xfff0) << 16)) + 0x100000;
    };
    this->pci_wr_io_base_upper16 = [this](uint16_t val) {
        if ((this->io_base & 15) == 1)
            this->io_base_upper16 = val;
        this->io_base_32 = ((uint32_t)this->io_base_upper16 << 16) |
                           ((this->io_base & 0xf0) << 8);
    };
    this->pci_wr_io_limit_upper16 = [this](uint16_t val) {
        if ((this->io_limit & 15) == 1)
            this->io_limit_upper16 = val;
        this->io_limit_32 = (((uint32_t)this->io_limit_upper16 << 16) |
                             ((this->io_limit & 0xf0) << 8)) + 0x1000;
    };

    this->pci_wr_pref_base_upper32  = [this](uint32_t val) {
        if ((this->pref_mem_cfg & 15) == 1)
            this->pref_base_upper32 = val;
        this->pref_mem_base_64 = ((uint64_t)this->pref_base_upper32 << 32) |
                                 ((this->pref_mem_base & 0xfff0) << 16);
    };

    this->pci_wr_pref_limit_upper32 = [this](uint32_t val) {
        if ((this->pref_mem_cfg & 15) == 1)
            this->pref_limit_upper32 = val;
        this->pref_mem_limit_64 = (((uint64_t)this->pref_limit_upper32 << 32) |
            ((this->pref_mem_limit & 0xfff0) << 16)) + 0x100000;
    };
};

uint32_t PCIBridge::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_BAR0:
    case PCI_CFG_BAR1:
        return this->bars[(reg_offs - 0x10) >> 2];
    case PCI_CFG_IO_BASE:
        return (this->pci_rd_sec_status() << 16) |
               (this->pci_rd_io_limit() << 8) | (this->pci_rd_io_base());
    case PCI_CFG_MEMORY_BASE:
        return (this->pci_rd_memory_limit() << 16) | (this->pci_rd_memory_base());
    case PCI_CFG_PREF_MEM_BASE:
        return (this->pci_rd_pref_mem_limit() << 16) | (this->pci_rd_pref_mem_base());
    case PCI_CFG_PREF_BASE_UPPER32:
        return this->pci_rd_pref_base_upper32();
    case PCI_CFG_PREF_LIMIT_UPPER32:
        return this->pci_rd_pref_limit_upper32();
    case PCI_CFG_IO_BASE_UPPER16:
        return (this->pci_rd_io_limit_upper16() << 16) | (this->pci_rd_io_base_upper16());
    case PCI_CFG_DWORD_13:
        return cap_ptr;
    case PCI_CFG_BRIDGE_ROM_ADDRESS:
        return exp_rom_bar;
    default:
        return PCIBridgeBase::pci_cfg_read(reg_offs, details);
    }
}

void PCIBridge::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_BAR0:
    case PCI_CFG_BAR1:
        this->set_bar_value((reg_offs - 0x10) >> 2, value);
        break;
    case PCI_CFG_IO_BASE:
        this->pci_wr_sec_status(value >> 16);
        this->pci_wr_io_limit(value >> 8);
        this->pci_wr_io_base(value & 0xFFU);
        break;
    case PCI_CFG_MEMORY_BASE:
        this->pci_wr_memory_limit(value >> 16);
        this->pci_wr_memory_base(value & 0xFFFFU);
        break;
    case PCI_CFG_PREF_MEM_BASE:
        this->pci_wr_pref_mem_limit(value >> 16);
        this->pci_wr_pref_mem_base(value & 0xFFFFU);
        break;
    case PCI_CFG_PREF_BASE_UPPER32:
        this->pci_wr_pref_base_upper32(value);
        break;
    case PCI_CFG_PREF_LIMIT_UPPER32:
        this->pci_wr_pref_limit_upper32(value);
        break;
    case PCI_CFG_IO_BASE_UPPER16:
        this->pci_wr_io_limit_upper16(value >> 16);
        this->pci_wr_io_base_upper16(value & 0xFFFFU);
        break;
    case PCI_CFG_BRIDGE_ROM_ADDRESS:
        this->pci_wr_exp_rom_bar(value);
        break;
    default:
        PCIBridgeBase::pci_cfg_write(reg_offs, value, details);
        break;
    }
}

bool PCIBridge::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res)
{
    if (!(this->command & 1))
        return false;

    if (offset < this->io_base_32 || offset + size >= this->io_limit_32)
        return false;

    return this->pci_io_read_loop(offset, size, *res);
}

bool PCIBridge::pci_io_write(uint32_t offset, uint32_t value, uint32_t size)
{
    if (!(this->command & 1))
        return false;

    if (offset < this->io_base_32 || offset + size >= this->io_limit_32)
        return false;

    return this->pci_io_write_loop(offset, size, value);
}
