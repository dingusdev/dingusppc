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

/** Bandit/Chaos ARBus-to-PCI Bridge emulation. */

#include <devices/common/pci/bandit.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/memctrlbase.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>

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
    switch (offset >> 22) {
    case 3: // CONFIG_DATA
        int bus_num, dev_num, fun_num;
        uint8_t reg_offs;
        AccessDetails details;
        PCIBase *device;
        cfg_setup(offset, size, bus_num, dev_num, fun_num, reg_offs, details, device);
        details.flags |= PCI_CONFIG_READ;
        if (device) {
            uint32_t value = device->pci_cfg_read(reg_offs, details);
            // bytes 4 to 7 are random on bandit but
            // we choose to repeat bytes 0 to 3 like grackle
            return pci_conv_rd_data(value, value, details);
        }
        LOG_READ_NON_EXISTENT_PCI_DEVICE();
        return 0xFFFFFFFFUL; // PCI spec §6.1

    case 2: // CONFIG_ADDR
        return BYTESWAP_32(this->config_addr);

    default: // I/O space
        return pci_io_read_broadcast(offset, size);
    }
}

void BanditHost::write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size)
{
    switch (offset >> 22) {
    case 3: // CONFIG_DATA
        int bus_num, dev_num, fun_num;
        uint8_t reg_offs;
        AccessDetails details;
        PCIBase *device;
        cfg_setup(offset, size, bus_num, dev_num, fun_num, reg_offs, details, device);
        details.flags |= PCI_CONFIG_WRITE;
        if (device) {
            if (size == 4 && !details.offset) { // aligned DWORD writes -> fast path
                device->pci_cfg_write(reg_offs, BYTESWAP_32(value), details);
                return;
            }
            // otherwise perform necessary data transformations -> slow path
            uint32_t old_val = details.size == 4 ? 0 : device->pci_cfg_read(reg_offs, details);
            uint32_t new_val = pci_conv_wr_data(old_val, value, details);
            device->pci_cfg_write(reg_offs, new_val, details);
            return;
        }
        LOG_WRITE_NON_EXISTENT_PCI_DEVICE();
        break;

    case 2: // CONFIG_ADDR
        this->config_addr = BYTESWAP_32(value);
        break;

    default: // I/O space
        pci_io_write_broadcast(offset, size, value);
    }
}

inline void BanditHost::cfg_setup(uint32_t offset, int size, int &bus_num,
                                  int &dev_num, int &fun_num, uint8_t &reg_offs,
                                  AccessDetails &details, PCIBase *&device)
{
    details.size = size;
    details.offset = offset & 3;
    fun_num = FUN_NUM();
    reg_offs = REG_NUM();
    if (this->config_addr & BANDIT_CAR_TYPE) { // type 1 configuration command
        details.flags = PCI_CONFIG_TYPE_1;
        bus_num = BUS_NUM();
        dev_num = DEV_NUM();
        device = pci_find_device(bus_num, dev_num, fun_num);
        return;
    }
    details.flags = PCI_CONFIG_TYPE_0;
    bus_num = 0; // use dummy value for bus number
    uint32_t idsel = this->config_addr & 0xFFFFF800U;
    if (!SINGLE_BIT_SET(idsel)) {
        for (dev_num = -1, idsel = this->config_addr; idsel; idsel >>= 1, dev_num++) {}
        LOG_F(ERROR, "%s: config_addr 0x%08x does not contain valid IDSEL",
              this->name.c_str(), (uint32_t)this->config_addr);
        device = NULL;
        return;
    }
    dev_num = WHAT_BIT_SET(idsel);
    device = pci_find_device(dev_num, fun_num);
}

int BanditHost::device_postinit()
{
    std::string pci_dev_name;

    static const std::map<std::string, int> pci_slots1 = {
        {"pci_A1", DEV_FUN(0xD,0)}, {"pci_B1", DEV_FUN(0xE,0)}, {"pci_C1", DEV_FUN(0xF,0)}
    };

    static const std::map<std::string, int> pci_slots2 = {
        {"pci_D2", DEV_FUN(0xD,0)}, {"pci_E2", DEV_FUN(0xE,0)}, {"pci_F2", DEV_FUN(0xF,0)}
    };

    static const std::map<std::string, int> vci_slots = {
        {"vci_D", DEV_FUN(0xD,0)}, {"vci_E", DEV_FUN(0xE,0)}, {"vci_F", DEV_FUN(0xF,0)}
    };

    for (auto& slot :
        this->bridge_num == 0 ? vci_slots  :
        this->bridge_num == 1 ? pci_slots1 :
        this->bridge_num == 2 ? pci_slots2 :
        pci_slots1
    ) {
        pci_dev_name = GET_STR_PROP(slot.first);
        if (!pci_dev_name.empty()) {
            this->attach_pci_device(pci_dev_name, slot.second);
        }
    }
    return 0;
}

Bandit::Bandit(int bridge_num, std::string name, int dev_id, int rev)
    : BanditHost(bridge_num)
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
    this->pci_register_device(DEV_FUN(BANDIT_DEV,0), this->my_pci_device.get());
}

Chaos::Chaos(std::string name) : BanditHost(0)
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

static const PropMap Bandit1_Properties = {
    {"pci_A1",
        new StrProperty("")},
    {"pci_B1",
        new StrProperty("")},
    {"pci_C1",
        new StrProperty("")},
};

static const PropMap Bandit2_Properties = {
    {"pci_D2",
        new StrProperty("")},
    {"pci_E2",
        new StrProperty("")},
    {"pci_F2",
        new StrProperty("")},
};

static const PropMap Chaos_Properties = {
    {"vci_D",
        new StrProperty("")},
    {"vci_E",
        new StrProperty("")},
    {"vci_F",
        new StrProperty("")},
};

static const DeviceDescription Bandit1_Descriptor = {
    Bandit::create_first, {}, Bandit1_Properties
};

static const DeviceDescription Bandit2_Descriptor = {
    Bandit::create_second, {}, Bandit2_Properties
};

static const DeviceDescription PsxPci1_Descriptor = {
    Bandit::create_psx_first, {}, Bandit1_Properties
};

static const DeviceDescription Chaos_Descriptor = {
    Chaos::create, {}, Chaos_Properties
};

REGISTER_DEVICE(Bandit1, Bandit1_Descriptor);
REGISTER_DEVICE(Bandit2, Bandit2_Descriptor);
REGISTER_DEVICE(PsxPci1, PsxPci1_Descriptor);
REGISTER_DEVICE(Chaos,   Chaos_Descriptor);
