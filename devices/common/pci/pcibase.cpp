/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#include <devices/common/pci/pcibase.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <cinttypes>
#include <fstream>
#include <cstring>
#include <string>

PCIBase::PCIBase(std::string name, PCIHeaderType hdr_type, int num_bars)
{
    this->name = name;
    this->hdr_type = hdr_type;
    this->num_bars = num_bars;

    this->pci_rd_stat       = [this]() { return this->status; };
    this->pci_rd_cmd        = [this]() { return this->command; };
    this->pci_rd_bist       = []() { return 0; };
    this->pci_rd_lat_timer  = [this]() { return this->lat_timer; };
    this->pci_rd_cache_lnsz = [this]() { return this->cache_ln_sz; };

    this->pci_wr_stat       = [this](uint16_t val) {
        this->status &= ~(0b1111100100000000 & val);
    };

    this->pci_wr_cmd        = [this](uint16_t cmd) {
        /*
            FIXME: should register or unregister BAR mmio regions if (cmd & 2) changes.
            Or the mmio regions should be enabled/disabled.
        */
        this->command = cmd & this->command_cfg;
    };
    this->pci_wr_bist       = [](uint8_t  val) {};
    this->pci_wr_lat_timer  = [this](uint8_t val) { this->lat_timer = val; };
    this->pci_wr_cache_lnsz = [this](uint8_t val) { this->cache_ln_sz = val; };

    this->pci_notify_bar_change = [](int bar_num) {};
}

uint32_t PCIBase::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_DEV_ID:
        return (this->device_id << 16) | (this->vendor_id);
    case PCI_CFG_STAT_CMD:
        return (this->pci_rd_stat() << 16) | (this->pci_rd_cmd());
    case PCI_CFG_CLASS_REV:
        return this->class_rev;
    case PCI_CFG_DWORD_3:
        return (pci_rd_bist() << 24) | (this->hdr_type << 16) |
               (pci_rd_lat_timer() << 8) | pci_rd_cache_lnsz();
    }
    LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    return 0;
}

void PCIBase::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_STAT_CMD:
        this->pci_wr_stat(value >> 16);
        this->pci_wr_cmd(value & 0xFFFFU);
        break;
    case PCI_CFG_DWORD_3:
        this->pci_wr_bist(value >> 24);
        this->pci_wr_lat_timer((value >> 8) & 0xFF);
        this->pci_wr_cache_lnsz(value & 0xFF);
        break;
    default:
        LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
    }
}

void PCIBase::setup_bars(std::vector<BarConfig> cfg_data)
{
    for (auto cfg_entry : cfg_data) {
        if (cfg_entry.bar_num > this->num_bars) {
            ABORT_F("BAR number %d out of range", cfg_entry.bar_num);
        }
        this->bars_cfg[cfg_entry.bar_num] = cfg_entry.bar_cfg;
    }

    this->finish_config_bars();
}

int PCIBase::attach_exp_rom_image(const std::string img_path)
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
        size_t exp_rom_image_size = img_file.tellg();
        if (exp_rom_image_size > 4*1024*1024) {
            throw std::runtime_error("expansion ROM file too large");
        }

        // verify PCI struct offset
        uint16_t pci_struct_offset = 0;
        img_file.seekg(0x18, std::ios::beg);
        img_file.read((char *)&pci_struct_offset, sizeof(pci_struct_offset));

        if (pci_struct_offset > exp_rom_image_size) {
            throw std::runtime_error("invalid PCI structure offset");
        }

        // verify PCI struct signature
        img_file.seekg(pci_struct_offset, std::ios::beg);
        img_file.read((char *)buf, sizeof(buf));

        if (buf[0] != 'P' || buf[1] != 'C' || buf[2] != 'I' || buf[3] != 'R') {
            throw std::runtime_error("unexpected PCI struct signature");
        }

        // find minimum rom size for the rom file (power of 2 >= 0x800)
        for (this->exp_rom_size = 1 << 11; this->exp_rom_size < exp_rom_image_size; this->exp_rom_size <<= 1) {}

        // ROM image ok - go ahead and load it
        this->exp_rom_data = std::unique_ptr<uint8_t[]> (new uint8_t[this->exp_rom_size]);
        img_file.seekg(0, std::ios::beg);
        img_file.read((char *)this->exp_rom_data.get(), exp_rom_image_size);
        memset(&this->exp_rom_data[exp_rom_image_size], 0xff, this->exp_rom_size - exp_rom_image_size);

        if (exp_rom_image_size == this->exp_rom_size) {
            LOG_F(INFO, "%s: loaded expansion rom (%d bytes).",
            this->get_name().c_str(), this->exp_rom_size);
        }
        else {
            LOG_F(WARNING, "%s: loaded expansion rom (%d bytes adjusted to %d bytes).",
            this->get_name().c_str(), (int)exp_rom_image_size, this->exp_rom_size);
        }

        this->exp_bar_cfg  = ~(this->exp_rom_size - 1);
    }
    catch (const std::exception& exc) {
        LOG_F(ERROR, "%s: %s", this->get_name().c_str(), exc.what());
        result = -1;
    }

    img_file.close();

    return result;
}

void PCIBase::set_bar_value(int bar_num, uint32_t value)
{
    uint32_t bar_cfg = this->bars_cfg[bar_num];
    switch (bars_typ[bar_num]) {
        case PCIBarType::Unused:
            return;

        case PCIBarType::Io_16_Bit:
        case PCIBarType::Io_32_Bit:
            this->bars[bar_num] = (value & bar_cfg & ~3) | (bar_cfg & 3);
            if (value != 0xFFFFFFFFUL && (value & ~3) != (value & bar_cfg & ~3)) {
                LOG_F(ERROR, "%s: BAR %d cannot be 0x%08x (set to 0x%08x)",
                    this->get_name().c_str(), bar_num, (value & ~3), (value & bar_cfg & ~3));
            }
            break;

        case PCIBarType::Mem_20_Bit:
        case PCIBarType::Mem_32_Bit:
        case PCIBarType::Mem_64_Bit_Lo:
            this->bars[bar_num] = (value & bar_cfg & ~0xF) | (bar_cfg & 0xF);
            if (value != 0xFFFFFFFFUL && (value & ~0xF) != (value & bar_cfg & ~0xF)) {
                LOG_F(ERROR, "%s: BAR %d cannot be 0x%08x (set to 0x%08x)",
                    this->get_name().c_str(), bar_num, (value & ~0xF), (value & bar_cfg & ~0xF));
            }
            break;

        case PCIBarType::Mem_64_Bit_Hi:
            this->bars[bar_num] = value & bar_cfg;
            break;
    }

    if (value != 0xFFFFFFFFUL) // don't notify the device during BAR sizing
        this->pci_notify_bar_change(bar_num);
}

void PCIBase::finish_config_bars()
{
    for (int bar_num = 0; bar_num < this->num_bars; bar_num++) {
        uint32_t bar_cfg = this->bars_cfg[bar_num];

        if (!bar_cfg) // skip unimplemented BARs
            continue;

        if (bar_cfg & 1) {
            bars_typ[bar_num] = (bar_cfg & 0xffff0000) ? PCIBarType::Io_32_Bit :
                                                         PCIBarType::Io_16_Bit;
            has_io_space = true;
        }
        else {
            int pci_space_type = (bar_cfg >> 1) & 3;
            switch (pci_space_type) {
            case 0:
                bars_typ[bar_num] = PCIBarType::Mem_32_Bit;
                break;
            case 1:
                bars_typ[bar_num] = PCIBarType::Mem_20_Bit;
                break;
            case 2:
                if (bar_num >= num_bars - 1) {
                    ABORT_F("%s: BAR %d cannot be 64-bit",
                            this->get_name().c_str(), bar_num);
                }
                else if (this->bars_cfg[bar_num+1] == 0) {
                    ABORT_F("%s: 64-bit BAR %d has zero for upper 32 bits",
                            this->get_name().c_str(), bar_num);
                }
                else {
                    bars_typ[bar_num++] = PCIBarType::Mem_64_Bit_Lo;
                    bars_typ[bar_num  ] = PCIBarType::Mem_64_Bit_Hi;
                }
                break;
            default:
                ABORT_F("%s: invalid or unsupported PCI space type %d for BAR %d",
                        this->get_name().c_str(), pci_space_type, bar_num);
            } // switch pci_space_type
        }
    } // for bar_num
}

void PCIBase::map_exp_rom_mem()
{
    uint32_t rom_addr = this->exp_rom_bar & this->exp_bar_cfg;
    if (rom_addr) {
        if (this->exp_rom_addr != rom_addr) {
            this->unmap_exp_rom_mem();
            uint32_t rom_size = ~this->exp_bar_cfg + 1;
        this->host_instance->pci_register_mmio_region(rom_addr, rom_size, this);
        this->exp_rom_addr = rom_addr;
    }
}
    else {
        this->unmap_exp_rom_mem();
    }
}

void PCIBase::unmap_exp_rom_mem()
{
    if (this->exp_rom_addr) {
        uint32_t rom_size = ~this->exp_bar_cfg + 1;
        this->host_instance->pci_unregister_mmio_region(exp_rom_addr, rom_size, this);
        this->exp_rom_addr = 0;
    }
}

void PCIBase::pci_wr_exp_rom_bar(uint32_t data)
{
    if (!this->exp_bar_cfg) {
        return;
    }

    if ((data & this->exp_bar_cfg) == this->exp_bar_cfg) {
        // doing sizing
        this->exp_rom_bar = (data & (this->exp_bar_cfg | 1));
    } else {
        this->exp_rom_bar = (data & (this->exp_bar_cfg | 1));
        if (this->exp_rom_bar & 1) {
            this->map_exp_rom_mem();
        }
        else {
            this->unmap_exp_rom_mem();
        }
    }
}

void PCIBase::pci_interrupt(uint8_t irq_line_state) {
    if (!(this->command & 0x0400)) {
        if (!this->int_details.int_ctrl_obj)
            this->host_instance->register_pci_int(this);
        if (this->int_details.int_ctrl_obj && this->int_details.irq_id)
            this->int_details.int_ctrl_obj->ack_int(this->int_details.irq_id, irq_line_state);
        else
            LOG_F(ERROR, "Unhandled interrupt from device %s", this->get_name().c_str());
        /* A pci device should set the interrupt status bit when an interrupt occurs
         * and it should clear the interrupt status bit when its interrupt bits are cleared.
         * The interrupt status bit is new to PCI Local Bus Specification Revision 2.3
         * and Power Macs use PCI Local Bus Specification Revision 2.0. Some PCI cards
         * might not implement this bit.
        */
        //this->status |= 0x0008;
    }
}
