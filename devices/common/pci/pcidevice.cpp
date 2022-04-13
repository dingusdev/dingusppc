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
#include <fstream>
#include <string>

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
            this->exp_rom_bar = (data & 0xFFFFF801UL);
            if (this->exp_rom_bar & 1) {
                this->map_exp_rom_mem(this->exp_rom_bar & 0xFFFFF800UL);
            } else {
                LOG_F(WARNING, "%s: unmapping of expansion ROM not implemented yet",
                      this->pci_name.c_str());
            }
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

int PCIDevice::attach_exp_rom_image(const std::string img_path)
{
    std::ifstream img_file;

    int result = 0;

    this->exp_bar_cfg = 0; // tell the world we got no ROM for now

    try {
        img_file.open(img_path, std::ios::in | std::ios::binary);
        if (img_file.fail()) {
            throw std::runtime_error("could not open specified ROM dump image");
        }

        // validate image file
        uint8_t buf[4] = { 0 };

        img_file.seekg(0, std::ios::beg);
        img_file.read((char *)buf, sizeof(buf));

        if (buf[0] != 0x55 || buf[1] != 0xAA) {
            throw std::runtime_error("invalid expansion ROM signature");
        }

        // determine image size
        img_file.seekg(0, std::ios::end);
        this->exp_rom_size = img_file.tellg();

        // verify PCI struct offset
        uint32_t pci_struct_offset = 0;
        img_file.seekg(0x18, std::ios::beg);
        img_file.read((char *)&pci_struct_offset, sizeof(pci_struct_offset));

        if (pci_struct_offset > this->exp_rom_size) {
            throw std::runtime_error("invalid PCI structure offset");
        }

        // verify PCI struct signature
        img_file.seekg(pci_struct_offset, std::ios::beg);
        img_file.read((char *)buf, sizeof(buf));

        if (buf[0] != 'P' || buf[1] != 'C' || buf[2] != 'I' || buf[3] != 'R') {
            throw std::runtime_error("unexpected PCI struct signature");
        }

        // ROM image ok - go ahead and load it
        this->exp_rom_data = std::unique_ptr<uint8_t[]> (new uint8_t[this->exp_rom_size]);
        img_file.seekg(0, std::ios::beg);
        img_file.read((char *)this->exp_rom_data.get(), this->exp_rom_size);

        // align ROM image size on a 2KB boundary and initialize ROM config
        this->exp_rom_size = (this->exp_rom_size + 0x7FFU) & 0xFFFFF800UL;
        this->exp_bar_cfg  = ~(this->exp_rom_size - 1);
    }
    catch (const std::exception& exc) {
        LOG_F(ERROR, "PCIDevice: %s", exc.what());
        result = -1;
    }

    img_file.close();

    return result;
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

void PCIDevice::map_exp_rom_mem(uint32_t rom_addr)
{
    if (!this->exp_rom_addr || this->exp_rom_addr != rom_addr) {
        this->host_instance->pci_register_mmio_region(rom_addr, 0x10000, this);
    }
}
