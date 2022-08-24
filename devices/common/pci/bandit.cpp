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

/** Bandit ARBus-to-PCI Bridge emulation. */

#include <devices/common/pci/bandit.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/memctrlbase.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cinttypes>

const int MultiplyDeBruijnBitPosition2[] =
{
    0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
    31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

/** finds the position of the bit that is set */
#define WHAT_BIT_SET(val) (MultiplyDeBruijnBitPosition2[(uint32_t)(val * 0x077CB531U) >> 27])

Bandit::Bandit(int bridge_num, std::string name) : PCIHost(), PCIDevice(name)
{
    supports_types(HWCompType::PCI_HOST | HWCompType::PCI_DEV);

    this->base_addr = 0xF0000000 + ((bridge_num & 3) << 25);

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add memory mapped I/O region for Bandit control registers
    // This region has the following layout:
    // base_addr +  0x000000 --> I/O space
    // base_addr +  0x800000 --> CONFIG_ADDR
    // base_addr +  0xC00000 --> CONFIG_DATA
    // base_addr + 0x1000000 --> pass-through memory space (not included below)
    mem_ctrl->add_mmio_region(base_addr, 0x01000000, this);

    // prepare the PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = 0x0001;
    this->class_rev   = 0x06000003;
    this->cache_ln_sz = 8;
    this->command     = 0x16;

    // make several PCI config space registers read-only
    this->pci_wr_cmd = [](uint16_t cmd) {}; // command register
    this->pci_wr_cache_lnsz = [](uint8_t val) {}; // cache line size register

    // set the bits in the fine address space field of the address mask register
    // that correspond to the 32MB assigned PCI address space of this Bandit.
    // This initialization is implied by the device functionality.
    this->addr_mask = 3 << ((bridge_num & 3) * 2);

    this->name = name;
}

uint32_t Bandit::pci_cfg_read(uint32_t reg_offs, uint32_t size)
{
    if (reg_offs < 64) {
        return PCIDevice::pci_cfg_read(reg_offs, size);
    }

    switch (reg_offs) {
    case BANDIT_ADDR_MASK:
        return BYTESWAP_32(this->addr_mask);
    default:
        LOG_F(WARNING, "%s: reading from unimplemented config register at 0x%X",
              this->pci_name.c_str(), reg_offs);
    }

    return 0;
}

void Bandit::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size)
{
    if (reg_offs < 64) {
        PCIDevice::pci_cfg_write(reg_offs, value, size);
        return;
    }

    switch (reg_offs) {
    case BANDIT_ADDR_MASK:
        this->addr_mask = BYTESWAP_32(value);
        this->verbose_address_space();
        break;
    default:
        LOG_F(WARNING, "%s: writing to unimplemented config register at 0x%X",
              this->pci_name.c_str(), reg_offs);
    }
}

uint32_t Bandit::read(uint32_t rgn_start, uint32_t offset, int size)
{
    int      bus_num, dev_num, fun_num;
    uint8_t  reg_offs;
    uint32_t result, idsel;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            fun_num = (this->config_addr >> 8) & 7;
            reg_offs = this->config_addr & 0xFCU;

            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_CAR_TYPE) {
                bus_num = (this->config_addr >> 16) & 255;
                dev_num = (this->config_addr >> 11) & 31;
                LOG_F(
                    WARNING, "%s: read config cycle type 1 not supported yet %02x:%02x.%x @%02x.%c",
                    this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
                );
                return 0xFFFFFFFFUL; // PCI spec §6.1
            }

            idsel = (this->config_addr >> 11) & 0x1FFFFFU;

            if (!SINGLE_BIT_SET(idsel)) {
                LOG_F(
                    ERROR, "%s: read invalid IDSEL=0x%X config:0x%X ??:??.%x? @%02x?.%c",
                    this->name.c_str(), idsel, this->config_addr,
                    fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
                );
                return 0xFFFFFFFFUL; // PCI spec §6.1
            }

            if (idsel == BANDIT_ID_SEL) { // access to myself
                result = this->pci_cfg_read(reg_offs, size);
            } else {
                if (this->dev_map.count(idsel)) {
                    result = this->dev_map[idsel]->pci_cfg_read(reg_offs, size);
                } else {
                    dev_num = WHAT_BIT_SET(idsel) + 11;
                    LOG_F(
                        ERROR, "%s err: read attempt from non-existing PCI device ??:%02x.%x @%02x.%c",
                        this->name.c_str(), dev_num, fun_num, reg_offs + (offset & 3),
                        size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
                    );
                    return 0xFFFFFFFFUL; // PCI spec §6.1
                }
            }
        } else {
            result = this->config_addr;
        }
    } else { // I/O space access
        // broadcast I/O request to devices that support I/O space
        // until a device returns true that means "request accepted"
        for (auto& dev : this->io_space_devs) {
            if (dev->pci_io_read(offset, size, &result)) {
                return result;
            }
        }
        LOG_F(ERROR, "%s: attempt to read from unmapped PCI I/O space, offset=0x%X",
              this->name.c_str(), offset);
    }
    return result;
}

void Bandit::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    int      bus_num, dev_num, fun_num;
    uint8_t  reg_offs;
    uint32_t idsel;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            fun_num = (this->config_addr >> 8) & 7;
            reg_offs = this->config_addr & 0xFCU;

            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_CAR_TYPE) {
                bus_num = (this->config_addr >> 16) & 255;
                dev_num = (this->config_addr >> 11) & 31;
                LOG_F(
                    WARNING, "%s: write config cycle type 1 not supported yet %02x:%02x.%x @%02x.%c = %0*x",
                    this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size, size * 2, value
                );
                return;
            }

            idsel = (this->config_addr >> 11) & 0x1FFFFFU;

            if (!SINGLE_BIT_SET(idsel)) {
                LOG_F(
                    ERROR, "%s: write invalid IDSEL=0x%X config:0x%X ??:??.%x? @%02x?.%c = %0*x",
                    this->name.c_str(), idsel, this->config_addr,
                    fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size, size * 2, value
                );
                return;
            }

            if (idsel == BANDIT_ID_SEL) { // access to myself
                this->pci_cfg_write(reg_offs, value, size);
                return;
            }

            if (this->dev_map.count(idsel)) {
                this->dev_map[idsel]->pci_cfg_write(reg_offs, value, size);
            } else {
                dev_num = WHAT_BIT_SET(idsel) + 11;
                LOG_F(
                    ERROR, "%s err: write attempt to non-existing PCI device ??:%02x.%x @%02x.%c = %0*x",
                    this->name.c_str(), dev_num, fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size, size * 2, value
                );
            }
        } else {
            this->config_addr = BYTESWAP_32(value);
        }
    } else { // I/O space access
        // broadcast I/O request to devices that support I/O space
        // until a device returns true that means "request accepted"
        for (auto& dev : this->io_space_devs) {
            if (dev->pci_io_write(offset, value, size)) {
                return;
            }
        }
        LOG_F(ERROR, "%s: attempt to write to unmapped PCI I/O space, offset=0x%X",
              this->name.c_str(), offset);
    }
}

void Bandit::verbose_address_space()
{
    uint32_t mask;
    int bit_pos;

    LOG_F(INFO, "%s address spaces:", this->pci_name.c_str());

    // verbose coarse aka 256MB memory regions
    for (mask = 0x10000, bit_pos = 0; mask != 0x80000000UL; mask <<= 1, bit_pos++) {
        if (this->addr_mask & mask) {
            uint32_t start_addr = bit_pos << 28;
            LOG_F(INFO, "- 0x%X ... 0x%X", start_addr, start_addr + 0x0FFFFFFFU);
        }
    }

    // verbose fine aka 16MB memory regions
    for (mask = 0x1, bit_pos = 0; mask != 0x10000UL; mask <<= 1, bit_pos++) {
        if (this->addr_mask & mask) {
            uint32_t start_addr = (bit_pos << 24) + 0xF0000000UL;
            LOG_F(INFO, "- 0x%X ... 0x%X", start_addr, start_addr + 0x00FFFFFFU);
        }
    }
}

Chaos::Chaos(std::string name) : PCIHost()
{
    supports_types(HWCompType::PCI_HOST);

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add memory mapped I/O region for Chaos control registers
    // This region has the following layout:
    // base_addr +  0x800000 --> CONFIG_ADDR
    // base_addr +  0xC00000 --> CONFIG_DATA
    mem_ctrl->add_mmio_region(0xF0000000UL, 0x01000000, this);

    this->name = name;
}

uint32_t Chaos::read(uint32_t rgn_start, uint32_t offset, int size)
{
    int      bus_num, dev_num, fun_num;
    uint8_t  reg_offs;
    uint32_t result, idsel;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            fun_num = (this->config_addr >> 8) & 7;
            reg_offs = this->config_addr & 0xFCU;

            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_CAR_TYPE) {
                bus_num = (this->config_addr >> 16) & 255;
                dev_num = (this->config_addr >> 11) & 31;
                LOG_F(
                    WARNING, "%s: read config cycle type 1 not supported yet %02x:%02x.%x @%02x.%c",
                    this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
                );
                return 0xFFFFFFFFUL; // PCI spec §6.1
            }

            idsel = (this->config_addr >> 11) & 0x1FFFFFU;

            if (!SINGLE_BIT_SET(idsel)) {
                LOG_F(
                    ERROR, "%s: read invalid IDSEL=0x%X config:0x%X ??:??.%x? @%02x?.%c",
                    this->name.c_str(), idsel, this->config_addr,
                    fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
                );
                return 0xFFFFFFFFUL; // PCI spec §6.1
            }

            if (this->dev_map.count(idsel)) {
                result = this->dev_map[idsel]->pci_cfg_read(reg_offs, size);
            } else {
                dev_num = WHAT_BIT_SET(idsel) + 11;
                LOG_F(
                    ERROR, "%s err: read attempt from non-existing VCI device ??:%02x.%x @%02x.%c",
                    this->name.c_str(), dev_num, fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
                );
                return 0xFFFFFFFFUL; // PCI spec §6.1
            }
        } else {
            result = this->config_addr;
        }
    } else { // I/O space access
        LOG_F(ERROR, "%s: I/O space not supported", this->name.c_str());
        return 0;
    }
    return result;
}

void Chaos::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    int      bus_num, dev_num, fun_num;
    uint8_t  reg_offs;
    uint32_t idsel;

    if (offset & BANDIT_CONFIG_SPACE) {
        if (offset & 0x00400000) {
            fun_num = (this->config_addr >> 8) & 7;
            reg_offs = this->config_addr & 0xFCU;

            // access to the CONFIG_DATA pseudo-register causes a Config Cycle
            if (this->config_addr & BANDIT_CAR_TYPE) {
                bus_num = (this->config_addr >> 16) & 255;
                dev_num = (this->config_addr >> 11) & 31;
                LOG_F(
                    WARNING, "%s: write config cycle type 1 not supported yet %02x:%02x.%x @%02x.%c = %0*x",
                    this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size, size * 2, value
                );
                return;
            }

            idsel = (this->config_addr >> 11) & 0x1FFFFFU;

            if (!SINGLE_BIT_SET(idsel)) {
                LOG_F(
                    ERROR, "%s: write invalid IDSEL=0x%X config:0x%X ??:??.%x? @%02x?.%c = %0*x",
                    this->name.c_str(), idsel, this->config_addr,
                    fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size, size * 2, value
                );
                return;
            }

            if (this->dev_map.count(idsel)) {
                this->dev_map[idsel]->pci_cfg_write(reg_offs, value, size);
            } else {
                dev_num = WHAT_BIT_SET(idsel) + 11;
                LOG_F(
                    ERROR, "%s err: write attempt to non-existing VCI device ??:%02x.%x @%02x.%c = %0*x",
                    this->name.c_str(), dev_num, fun_num, reg_offs + (offset & 3),
                    size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size, size * 2, value
                );
            }
        } else {
            this->config_addr = BYTESWAP_32(value);
        }
    } else { // I/O space access
        LOG_F(ERROR, "%s: I/O space not supported", this->name.c_str());
    }
}

static const DeviceDescription Bandit1_Descriptor = {
    Bandit::create_first, {}, {}
};

static const DeviceDescription Chaos_Descriptor = {
    Chaos::create, {}, {}
};

REGISTER_DEVICE(Bandit1, Bandit1_Descriptor);
REGISTER_DEVICE(Chaos, Chaos_Descriptor);
