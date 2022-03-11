/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

#include <devices/common/pci/pcidevice.h>
#include <devices/common/viacuda.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>

PCIDevice::PCIDevice(std::string name)
{
    this->pci_name = name;

    this->pci_rd_stat       = []() { return 0; };
    this->pci_rd_cmd        = [this]() { return this->command; };
    this->pci_rd_bist       = []() { return 0; };
    this->pci_rd_lat_timer  = [this]() { return this->lat_timer; };
    this->pci_rd_cache_lnsz = [this]() { return this->cache_ln_sz; };

    this->pci_wr_stat       = [](uint16_t val) {};
    this->pci_wr_cmd        = [this](uint16_t cmd) { this->command = cmd; };
    this->pci_wr_bist       = [](uint8_t  val) {};
    this->pci_wr_lat_timer  = [this](uint8_t val) { this->lat_timer = val; };
    this->pci_wr_cache_lnsz = [this](uint8_t val) { this->cache_ln_sz = val; };

    this->pci_notify_bar_change = [](int bar_num) {};
};

uint32_t PCIDevice::pci_cfg_read(uint32_t reg_offs, uint32_t size)
{
    uint32_t result;

    switch (reg_offs) {
    case PCI_CFG_DEV_ID:
        result = (this->device_id << 16) | (this->vendor_id);
        break;
    case PCI_CFG_STAT_CMD:
        result = (this->pci_rd_stat() << 16) | (this->pci_rd_cmd());
        break;
    case PCI_CFG_CLASS_REV:
        result = this->class_rev;
        break;
    case PCI_CFG_DWORD_3:
        result = (pci_rd_bist() << 24) | (this->hdr_type << 16) |
                 (pci_rd_lat_timer() << 8) | pci_rd_cache_lnsz();
        break;
    case PCI_CFG_BAR0:
    case PCI_CFG_BAR1:
    case PCI_CFG_BAR2:
    case PCI_CFG_BAR3:
    case PCI_CFG_BAR4:
    case PCI_CFG_BAR5:
        result = this->bars[(reg_offs - 0x10) >> 2];
        break;
    case PCI_CFG_SUBSYS_ID:
        result = (this->subsys_id << 16) | (this->subsys_vndr);
        break;
    case PCI_CFG_ROM_BAR:
        result = this->exp_rom_bar;
        break;
    case PCI_CFG_DWORD_15:
        result = (max_lat << 24) | (min_gnt << 16) | (irq_pin << 8) | irq_line;
        break;
    default:
        LOG_F(WARNING, "%s: attempt to read from reserved/unimplemented register %d",
              this->pci_name.c_str(), reg_offs);
        return 0;
    }

    if (size == 4) {
        return BYTESWAP_32(result);
    } else {
        return read_mem_rev(((uint8_t *)&result) + (reg_offs & 3), size);
    }
}

void PCIDevice::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size)
{
    uint32_t data;

    if (size == 4) {
        data = BYTESWAP_32(value);
    } else {
        // get current register content as DWORD and update it partially
        data = BYTESWAP_32(this->pci_cfg_read(reg_offs, 4));
        write_mem_rev(((uint8_t *)&data) + (reg_offs & 3), value, size);
    }

    switch (reg_offs) {
    case PCI_CFG_STAT_CMD:
        this->pci_wr_stat(data >> 16);
        this->pci_wr_cmd(data & 0xFFFFU);
        break;
    case PCI_CFG_DWORD_3:
        this->pci_wr_bist(data >> 24);
        this->pci_wr_lat_timer((data >> 8) & 0xFF);
        this->pci_wr_cache_lnsz(data & 0xFF);
        break;
    case PCI_CFG_BAR0:
    case PCI_CFG_BAR1:
    case PCI_CFG_BAR2:
    case PCI_CFG_BAR3:
    case PCI_CFG_BAR4:
    case PCI_CFG_BAR5:
        if (data == 0xFFFFFFFFUL) {
            this->do_bar_sizing((reg_offs - 0x10) >> 2);
        } else {
            this->set_bar_value((reg_offs - 0x10) >> 2, data);
        }
        break;
    case PCI_CFG_ROM_BAR:
        if (data == 0xFFFFF800UL) {
            this->exp_rom_bar = this->exp_bar_cfg;
        } else {
            this->exp_rom_bar = (data & 0xFFFFF800UL) | (this->exp_bar_cfg & 1);
        }
        break;
    case PCI_CFG_DWORD_15:
        this->irq_line = data >> 24;
        break;
    default:
        LOG_F(WARNING, "%s: attempt to write to reserved/unimplemented register %d",
              this->pci_name.c_str(), reg_offs);
    }
}

void PCIDevice::do_bar_sizing(int bar_num)
{
    this->bars[bar_num] = this->bars_cfg[bar_num];
}

void PCIDevice::set_bar_value(int bar_num, uint32_t value)
{
    uint32_t bar_cfg = this->bars_cfg[bar_num];
    if (bar_cfg & 1) {
        this->bars[bar_num] = (value & 0xFFFFFFFCUL) | 1;
    } else {
        if (bar_cfg & 6) {
            ABORT_F("Invalid or unsupported PCI space type: %d", (bar_cfg >> 1) & 3);
        }
        this->bars[bar_num] = (value & 0xFFFFFFF0UL) | (bar_cfg & 0xF);
    }
    this->pci_notify_bar_change(bar_num);
}
