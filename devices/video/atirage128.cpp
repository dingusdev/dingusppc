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

#include <core/bitops.h>
#include <core/timermanager.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/deviceregistry.h>
#include <devices/video/atirage128.h>
#include <devices/video/displayid.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <memaccess.h>

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>

/* Human readable ATI Rage 128 HW register names for easier debugging. */
static const std::map<uint16_t, std::string> rage128_reg_names = {
    {0x0000, "ATI_MM_INDEX"},         {0x0004, "ATI_MM_DATA"},
    {0x0008, "ATI_CLOCK_CNTL_INDEX"}, {0x000C, "ATI_CLOCK_CNTL_DATA"},
    {0x0010, "ATI_BIOS_0_SCRATCH"},   {0x0014, "ATI_BIOS_1_SCRATCH"},
    {0x0018, "ATI_BIOS_2_SCRATCH"},   {0x001C, "ATI_BIOS_3_SCRATCH"},
    {0x0030, "ATI_BUS_CNTL"},
    {0x0034, "ATI_BUS_CNTL1"},        {0x0040, "ATI_GEN_INT_CNTL"},
    {0x0044, "ATI_GEN_INT_STATUS"},   {0x0050, "ATI_CRTC_GEN_CNTL"},
    {0x0054, "ATI_CRTC_EXT_CNTL"},    {0x0058, "ATI_DAC_CNTL"},
    {0x005C, "ATI_CRTC_STATUS"},      {0x0068, "ATI_GPIO_MONID"},
    {0x006C, "ATI_SEPROM_CNTL"},      {0x0094, "ATI_I2C_CNTL_1"},
    {0x00B0, "ATI_PALETTE_INDEX"},    {0x00B4, "ATI_PALETTE_DATA"},
    {0x00E0, "ATI_CONFIG_CNTL"},      {0x00F0, "ATI_GEN_RESET_CNTL"},
    {0x00F4, "ATI_GEN_STATUS"},       {0x00F8, "ATI_CNFG_MEMSIZE"},
    {0x0130, "ATI_HOST_PATH_CNTL"},   {0x0140, "ATI_MEM_CNTL"},
    {0x0144, "ATI_EXT_MEM_CNTL"},     {0x05D0, "ATI_GUI_STAT"},
};

ATIRage128::ATIRage128(uint16_t dev_id) : PCIDevice("ati-rage128"), VideoCtrlBase(640, 480) {
    uint8_t asic_id;

    supports_types(HWCompType::MMIO_DEV | HWCompType::PCI_DEV);

    this->vram_size = GET_INT_PROP("gfxmem_size") << 20;    // convert MBs to bytes

    // allocate video RAM
    this->vram_ptr = std::unique_ptr<uint8_t[]>(new uint8_t[this->vram_size]);

    // ATI Rage driver needs to know ASIC ID (manufacturer's internal chip code)
    // to operate properly
    switch (dev_id) {
    case ATI_RAGE_128_PRO_DEV_ID:
        asic_id = 0x50;
        break;
    default:
        asic_id = 0xDD;
        LOG_F(WARNING, "ATI Rage 128: bogus ASIC ID assigned!");
    }

    // set up PCI configuration space header
    this->vendor_id   = PCI_VENDOR_ATI;
    this->device_id   = dev_id;
    this->subsys_vndr = PCI_VENDOR_ATI;
    this->subsys_id   = 0x6987;    // adapter ID
    this->class_rev   = (0x030000 << 8) | asic_id;
    this->min_gnt     = 8;
    this->irq_pin     = 1;

    for (int i = 0; i < this->aperture_count; i++) {
        this->bars_cfg[i] = (uint32_t)(-this->aperture_size[i] | this->aperture_flag[i]);
    }

    this->finish_config_bars();

    //this->pci_notify_bar_change = [this](int bar_num) { 
    //    this->notify_bar_change(bar_num); 
    //};

    // stuff default values into chip registers
    //this->regs[ATI_CONFIG_CHIP_ID] = (asic_id << 24) | dev_id;

    // initialize display identification
    this->disp_id = std::unique_ptr<DisplayID>(new DisplayID());

    //uint8_t mon_code = this->disp_id->read_monitor_sense(0, 0);

    //this->regs[ATI_GP_IO] = ((mon_code & 6) << 11) | ((mon_code & 1) << 8);
}

const char* ATIRage128::get_reg_name(uint32_t reg_offset) {
    auto iter = rage128_reg_names.find(reg_offset & ~3);
    if (iter != rage128_reg_names.end()) {
        return iter->second.c_str();
    } else {
        return "unknown Rage 128 register";
    }
}

void ATIRage128::change_one_bar(
    uint32_t& aperture, uint32_t aperture_size, uint32_t aperture_new, int bar_num) {
    if (aperture != aperture_new) {
        if (aperture)
            this->host_instance->pci_unregister_mmio_region(aperture, aperture_size, this);

        aperture = aperture_new;
        if (aperture)
            this->host_instance->pci_register_mmio_region(aperture, aperture_size, this);

        LOG_F(INFO, "%s: aperture[%d] set to 0x%08X", this->name.c_str(), bar_num, aperture);
    }
}

uint32_t ATIRage128::read_reg(uint32_t reg_offset, uint32_t size) {
    uint64_t result;
    uint32_t offset = reg_offset & 3;

    switch (reg_offset >> 2) {
    default:
        result = this->regs[reg_offset >> 2];
    }

    if (!offset && size == 4) {    // fast path
        return result;
    } else {    // slow path
        if ((offset + size) > 4) {
            result |= (uint64_t)(this->regs[(reg_offset >> 2) + 1]) << 32;
        }
        return extract_bits<uint64_t>(result, offset * 8, size * 8);
    }
}

uint32_t ATIRage128::pci_cfg_read(uint32_t reg_offs, AccessDetails& details) {
    if (reg_offs < 128) {
        return PCIDevice::pci_cfg_read(reg_offs, details);
    }

    LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER();

    return 0;
}