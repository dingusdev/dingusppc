/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** Bandit/Chaos ARBus-to-PCI Bridge emulation. */

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

BanditPciDevice::BanditPciDevice(int bridge_num, std::string name, int dev_id, int rev)
    : PCIDevice(name)
{
    supports_types(HWCompType::PCI_DEV);

    // prepare the PCI config header
    this->vendor_id   = PCI_VENDOR_APPLE;
    this->device_id   = dev_id;
    this->class_rev   = 0x06000000 | (rev & 0xFFU);
    this->cache_ln_sz = 8;
    this->command     = 0x16;

    // make several PCI config space registers read-only
    this->pci_wr_cmd = [](uint16_t cmd) {}; // command register
    this->pci_wr_cache_lnsz = [](uint8_t val) {}; // cache line size register

    // set the bits in the fine address space field of the address mask register
    // that correspond to the 32MB assigned PCI address space of this Bandit.
    // This initialization is implied by the device functionality.
    this->addr_mask = 3 << ((bridge_num & 3) * 2);

    // initial PCI number + chip mode: big endian, interrupts & VGA space disabled
    this->mode_ctrl = ((bridge_num & 3) << 2) | 3;

    this->rd_hold_off_cnt = 8;
}

uint32_t BanditPciDevice::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    if (reg_offs < 64) {
        return PCIDevice::pci_cfg_read(reg_offs, details);
    }

    switch (reg_offs) {
    case BANDIT_ADDR_MASK:
        return this->addr_mask;
    case BANDIT_MODE_SELECT:
        return this->mode_ctrl;
    case BANDIT_ARBUS_RD_HOLD_OFF:
        return this->rd_hold_off_cnt;
    default:
        LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();
    }
    return 0;
}

void BanditPciDevice::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    if (reg_offs < 64) {
        PCIDevice::pci_cfg_write(reg_offs, value, details);
        return;
    }

    switch (reg_offs) {
    case BANDIT_ADDR_MASK:
        this->addr_mask = value;
        this->verbose_address_space();
        return;
    case BANDIT_MODE_SELECT:
        this->mode_ctrl = value;
        return;
    case BANDIT_ARBUS_RD_HOLD_OFF:
        this->rd_hold_off_cnt = value & 0x1F;
        return;
    default:
        LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER();
    }
}

void BanditPciDevice::verbose_address_space()
{
    uint32_t mask;
    int bit_pos;

    if (!this->addr_mask) {
        return;
    }

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

uint32_t BanditHost::read(uint32_t rgn_start, uint32_t offset, int size)
{
    uint32_t idsel, result;

    switch (offset >> 22) {
    case 3: // CONFIG_DATA
        if (this->config_addr & BANDIT_CAR_TYPE) { // type 1 configuration command
            LOG_F(
                WARNING, "%s: read config cycle type 1 not supported yet %02x:%02x.%x @%02x",
                this->name.c_str(), BUS_NUM(), DEV_NUM(), FUN_NUM(), offset & 0xFCU
            );
            return 0xFFFFFFFFUL; // PCI spec §6.1
        }

        idsel = (this->config_addr >> 11) & 0x1FFFFFU;

        if (!SINGLE_BIT_SET(idsel)) {
            LOG_F(ERROR, "%s: config_addr 0x%08x does not contain valid IDSEL",
                  this->name.c_str(), (uint32_t)this->config_addr);
            return 0xFFFFFFFFUL; // PCI spec §6.1
        }

        if (this->dev_map.count(idsel)) {
            AccessDetails details;
            details.offset = offset & 3;
            details.size   = size;
            details.flags  = PCI_CONFIG_TYPE_0 | PCI_CONFIG_READ;

            result = this->dev_map[idsel]->pci_cfg_read(REG_NUM(), details);
            return pci_cfg_rev_read(result, details);
        } else {
            LOG_F(
                ERROR, "%s err: read attempt from non-existing PCI device ??:%02x.%x @%02x",
                this->name.c_str(), WHAT_BIT_SET(idsel), FUN_NUM(), offset
            );
            return 0xFFFFFFFFUL; // PCI spec §6.1
        }
        break;

    case 2: // CONFIG_ADDR
        return BYTESWAP_32(this->config_addr);

    default: // I/O space
        // broadcast I/O request to devices that support I/O space
        // until a device returns true that means "request accepted"
        for (auto& dev : this->io_space_devs) {
            if (dev->pci_io_read(offset, size, &result))
                return result;
        }
        LOG_F(ERROR, "%s: attempt to read from unmapped PCI I/O space, offset=0x%X",
                      this->name.c_str(), offset);
    }

    return 0xFFFFFFFFUL; // PCI spec §6.1
}

void BanditHost::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    uint32_t idsel;

    switch (offset >> 22) {
    case 3: // CONFIG_DATA
        if (this->config_addr & BANDIT_CAR_TYPE) { // type 1 configuration command
            LOG_F(
                WARNING, "%s: write config cycle type 1 not supported yet %02x:%02x.%x @%02x",
                this->name.c_str(), BUS_NUM(), DEV_NUM(), FUN_NUM(), offset & 0xFCU
            );
            return;
        }

        idsel = (this->config_addr >> 11) & 0x1FFFFFU;

        if (!SINGLE_BIT_SET(idsel)) {
            LOG_F(ERROR, "%s: config_addr 0x%08x does not contain valid IDSEL",
                  this->name.c_str(), (uint32_t)this->config_addr);
            return;
        }

        if (this->dev_map.count(idsel)) {
            AccessDetails details;
            details.offset = offset & 3;
            details.size   = size;
            details.flags  = PCI_CONFIG_TYPE_0 | PCI_CONFIG_WRITE;

            if (size == 4 && !details.offset) { // aligned DWORD writes -> fast path
                this->dev_map[idsel]->pci_cfg_write(REG_NUM(), BYTESWAP_32(value), details);
            } else { // otherwise perform necessary data transformations -> slow path
                uint32_t old_val = this->dev_map[idsel]->pci_cfg_read(REG_NUM(), details);
                uint32_t new_val = pci_cfg_rev_write(old_val, value, details);
                this->dev_map[idsel]->pci_cfg_write(REG_NUM(), new_val, details);
            }
        } else {
            LOG_F(
                ERROR, "%s err: write attempt to non-existing PCI device ??:%02x.%x @%02x",
                this->name.c_str(), WHAT_BIT_SET(idsel), FUN_NUM(), offset
            );
        }
        break;

    case 2: // CONFIG_ADDR
        this->config_addr = BYTESWAP_32(value);
        break;

    default: // I/O space
        // broadcast I/O request to devices that support I/O space
        // until a device returns true that means "request handled"
        for (auto& dev : this->io_space_devs) {
            if (dev->pci_io_write(offset, value, size))
                return;
        }
        LOG_F(ERROR, "%s: attempt to read from unmapped PCI I/O space, offset=0x%X",
                      this->name.c_str(), offset);
    }
}

Bandit::Bandit(int bridge_num, std::string name, int dev_id, int rev)
    : BanditHost()
{
    this->name = name;

    supports_types(HWCompType::PCI_HOST);

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

    // connnect Bandit PCI device
	this->my_pci_device = unique_ptr<BanditPciDevice>(
        new BanditPciDevice(bridge_num, name, dev_id, rev)
    );
    this->pci_register_device(1, this->my_pci_device.get());
}

Chaos::Chaos(std::string name) : BanditHost()
{
    this->name = name;

    supports_types(HWCompType::PCI_HOST);

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    // add memory mapped I/O region for Chaos control registers
    // This region has the following layout:
    // base_addr +  0x800000 --> CONFIG_ADDR
    // base_addr +  0xC00000 --> CONFIG_DATA
    mem_ctrl->add_mmio_region(0xF0000000UL, 0x01000000, this);
}

static const DeviceDescription Bandit1_Descriptor = {
    Bandit::create_first, {}, {}
};

static const DeviceDescription PsxPci1_Descriptor = {
    Bandit::create_psx_first, {}, {}
};

static const DeviceDescription Chaos_Descriptor = {
    Chaos::create, {}, {}
};

REGISTER_DEVICE(Bandit1, Bandit1_Descriptor);
REGISTER_DEVICE(PsxPci1, PsxPci1_Descriptor);
REGISTER_DEVICE(Chaos,   Chaos_Descriptor);
