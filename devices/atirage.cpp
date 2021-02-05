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

#include "displayid.h"
#include "endianswap.h"
#include "memaccess.h"
#include "pcidevice.h"
#include <atirage.h>
#include <cstdint>
#include <map>
#include <thirdparty/loguru/loguru.hpp>

/* Human readable Mach64 HW register names for easier debugging. */
static const std::map<uint16_t, std::string> mach64_reg_names = {
    {0x0000, "CRTC_H_TOTAL_DISP"},
    {0x0004, "CRTC_H_SYNC_STRT_WID"},
    {0x0008, "CRTC_V_TOTAL_DISP"},
    {0x000C, "CRTC_V_SYNC_STRT_WID"},
    {0x0010, "CRTC_VLINE_CRNT_VLINE"},
    {0x0014, "CRTC_OFF_PITCH"},
    {0x0018, "CRTC_INT_CNTL"},
    {0x001C, "CRTC_GEN_CNTL"},
    {0x0020, "DSP_CONFIG"},
    {0x0024, "DSP_ON_OFF"},
    {0x002C, "MEM_BUF_CNTL"},
    {0x0034, "MEM_ADDR_CFG"},
    {0x0040, "OVR_CLR"},
    {0x0044, "OVR_WID_LEFT_RIGHT"},
    {0x0048, "OVR_WID_TOP_BOTTOM"},
    {0x0078, "GP_IO"},
    {0x007C, "HW_DEBUG"},
    {0x0080, "SCRATCH_REG0"},
    {0x0084, "SCRATCH_REG1"},
    {0x0088, "SCRATCH_REG2"},
    {0x008C, "SCRATCH_REG3"},
    {0x0090, "CLOCK_CNTL"},
    {0x00A0, "BUS_CNTL"},
    {0x00AC, "EXT_MEM_CNTL"},
    {0x00B0, "MEM_CNTL"},
    {0x00C0, "DAC_REGS"},
    {0x00C4, "DAC_CNTL"},
    {0x00D0, "GEN_TEST_CNTL"},
    {0x00D4, "CUSTOM_MACRO_CNTL"},
    {0x00E0, "CONFIG_CHIP_ID"},
    {0x00E4, "CONFIG_STAT0"},
    {0x01B4, "SRC_CNTL"},
    {0x01FC, "SCALE_3D_CNTL"},
    {0x0310, "FIFO_STAT"},
    {0x0338, "GUI_STAT"},
};


ATIRage::ATIRage(uint16_t dev_id, uint32_t mem_amount) : PCIDevice("ati-rage") {
    uint8_t asic_id;

    this->vram_size = mem_amount << 20;

    /* allocate video RAM */
    this->vram_ptr = new uint8_t[this->vram_size];

    /* ATI Rage driver needs to know ASIC ID (manufacturer's internal chip code)
       to operate properly */
    switch (dev_id) {
    case ATI_RAGE_PRO_DEV_ID:
        asic_id = 0x5C; // R3B/D/P-A4 fabricated by UMC
        break;
    default:
        asic_id = 0xDD;
        LOG_F(WARNING, "ATI Rage: bogus ASIC ID assigned!");
    }

    /* set up PCI configuration space header */
    WRITE_DWORD_LE_A(&this->pci_cfg[0], (dev_id << 16) | ATI_PCI_VENDOR_ID);
    WRITE_DWORD_LE_A(&this->pci_cfg[8], (0x030000 << 8) | asic_id);
    WRITE_DWORD_LE_A(&this->pci_cfg[0x3C], 0x00080100);

    /* stuff default values into chip registers */
    WRITE_DWORD_LE_A(&this->block_io_regs[ATI_CONFIG_CHIP_ID],
                    (asic_id << 24) | dev_id);

    /* initialize display identification */
    this->disp_id = new DisplayID();
}

ATIRage::~ATIRage()
{
    if (this->vram_ptr) {
        delete this->vram_ptr;
    }

    delete (this->disp_id);
}

const char* ATIRage::get_reg_name(uint32_t reg_offset) {
    auto iter = mach64_reg_names.find(reg_offset & ~3);
    if (iter != mach64_reg_names.end()) {
        return iter->second.c_str();
    } else {
        return "unknown Mach64 register";
    }
}

uint32_t ATIRage::read_reg(uint32_t offset, uint32_t size) {
    uint32_t res;

    switch (offset & ~3) {
    case ATI_GP_IO:
        break;
    case ATI_CLOCK_CNTL:
        /* reading from internal PLL registers */
        if (offset == ATI_CLOCK_CNTL+2 && size == 1 &&
            !(this->block_io_regs[ATI_CLOCK_CNTL+1] & 0x2)) {
            return this->plls[this->block_io_regs[ATI_CLOCK_CNTL+1] >> 2];
        }
        break;
    case ATI_DAC_REGS:
        if (offset == ATI_DAC_DATA) {
            this->block_io_regs[ATI_DAC_DATA] =
                this->palette[this->block_io_regs[ATI_DAC_R_INDEX]][this->comp_index];
            this->comp_index++; /* move to next color component */
            if (this->comp_index >= 3) {
                /* autoincrement reading index - move to next palette entry */
                (this->block_io_regs[ATI_DAC_R_INDEX])++;
                this->comp_index = 0;
            }
        }
        break;
    default:
        LOG_F(
            INFO,
            "ATI Rage: read I/O reg %s at 0x%X, size=%d, val=0x%X",
            get_reg_name(offset),
            offset,
            size,
            read_mem(&this->block_io_regs[offset], size));
    }

    if (offset > sizeof(this->block_io_regs)) {
        LOG_F(WARNING, "ATI Rage: register offset 0x%04X out of bounds!", offset);
        return 0;
    }

    res = read_mem(&this->block_io_regs[offset], size);

    return res;
}

void ATIRage::write_reg(uint32_t offset, uint32_t value, uint32_t size) {
    uint32_t gpio_val;
    uint16_t gpio_dir;

    if (offset > sizeof(this->block_io_regs)) {
        LOG_F(WARNING, "ATI Rage: register offset 0x%04X out of bounds!", offset);
        return;
    }

    /* size-dependent endian conversion */
    write_mem(&this->block_io_regs[offset], value, size);

    switch (offset & ~3) {
    case ATI_GP_IO:
        if (offset < (ATI_GP_IO + 2)) {
            gpio_val = READ_DWORD_LE_A(&this->block_io_regs[ATI_GP_IO]);
            gpio_dir = (gpio_val >> 16) & 0x3FFF;
            WRITE_WORD_LE_A(
                &this->block_io_regs[ATI_GP_IO],
                this->disp_id->read_monitor_sense(gpio_val, gpio_dir));
        }
        break;
    case ATI_CLOCK_CNTL:
        /* writing to internal PLL registers */
        if (offset == ATI_CLOCK_CNTL+2 && size == 1 &&
            (this->block_io_regs[ATI_CLOCK_CNTL+1] & 0x2)) {
            int pll_addr = this->block_io_regs[ATI_CLOCK_CNTL+1] >> 2;
            uint8_t pll_data = this->block_io_regs[ATI_CLOCK_CNTL+2];
            this->plls[pll_addr] = pll_data;
            LOG_F(INFO, "ATI Rage: PLL #%d set to 0x%02X", pll_addr, pll_data);
        }
        break;
    case ATI_DAC_REGS:
        switch (offset) {
        /* writing to read/write index registers resets color component index */
        case ATI_DAC_W_INDEX:
        case ATI_DAC_R_INDEX:
            this->comp_index = 0;
            break;
        case ATI_DAC_DATA:
            this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][this->comp_index] = value & 0xFF;
            this->comp_index++; /* move to next color component */
            if (this->comp_index >= 3) {
                LOG_F(
                    INFO,
                    "ATI DAC palette entry #%d set to R=%X, G=%X, B=%X",
                    this->block_io_regs[ATI_DAC_W_INDEX],
                    this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][0],
                    this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][1],
                    this->palette[this->block_io_regs[ATI_DAC_W_INDEX]][2]);
                /* autoincrement writing index - move to next palette entry */
                (this->block_io_regs[ATI_DAC_W_INDEX])++;
                this->comp_index = 0;
            }
        }
        break;
    default:
        LOG_F(
            INFO,
            "ATI Rage: %s register at 0x%X set to 0x%X",
            get_reg_name(offset),
            offset & ~3,
            READ_DWORD_LE_A(&this->block_io_regs[offset & ~3]));
    }
}


uint32_t ATIRage::pci_cfg_read(uint32_t reg_offs, uint32_t size) {
    uint32_t res = 0;

    LOG_F(INFO, "Reading ATI Rage config space, offset = 0x%X, size=%d", reg_offs, size);

    res = read_mem(&this->pci_cfg[reg_offs], size);

    LOG_F(INFO, "Return value: 0x%X", res);
    return res;
}

void ATIRage::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size) {
    LOG_F(
        INFO,
        "Writing into ATI Rage PCI config space, offset = 0x%X, val=0x%X size=%d",
        reg_offs,
        BYTESWAP_32(value),
        size);

    switch (reg_offs) {
    case 0x10: /* BAR 0 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR0], 0xFF000000UL);
        }
        else {
            this->aperture_base = BYTESWAP_32(value);
            LOG_F(INFO, "ATI Rage aperture address set to 0x%08X", this->aperture_base);
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR0], value);
            this->host_instance->pci_register_mmio_region(this->aperture_base,
                APERTURE_SIZE, this);
        }
        break;
    case 0x14: /* BAR 1: I/O space base, 256 bytes wide */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR1], 0xFFFFFF01UL);
        }
        else {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR1], value);
        }
        break;
    case 0x18: /* BAR 2 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR2], 0xFFFFF000UL);
        }
        else {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR2], value);
        }
        break;
    case CFG_REG_BAR3: /* unimplemented */
    case CFG_REG_BAR4: /* unimplemented */
    case CFG_REG_BAR5: /* unimplemented */
        WRITE_DWORD_BE_A(&this->pci_cfg[reg_offs], 0);
        break;
    case CFG_EXP_BASE: /* no expansion ROM */
        if (value == 0x00F8FFFFUL) {
            // return 0 (not implemented) when attempting to size the expansion ROM
            WRITE_DWORD_BE_A(&this->pci_cfg[reg_offs], 0);
        } else {
            WRITE_DWORD_BE_A(&this->pci_cfg[reg_offs], value);
        }
        break;
    default:
        write_mem(&this->pci_cfg[reg_offs], value, size);
    }
}


bool ATIRage::io_access_allowed(uint32_t offset, uint32_t* p_io_base) {
    if (!(this->pci_cfg[CFG_REG_CMD] & 1)) {
        LOG_F(WARNING, "ATI I/O space disabled in the command reg");
        return false;
    }

    uint32_t io_base = READ_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR1]) & ~3;

    if (offset < io_base || offset > (io_base + 0x100)) {
        LOG_F(WARNING, "Rage: I/O out of range, base=0x%X, offset=0x%X", io_base, offset);
        return false;
    }

    *p_io_base = io_base;

    return true;
}


bool ATIRage::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
    uint32_t io_base;

    if (!this->io_access_allowed(offset, &io_base)) {
        return false;
    }

    *res = this->read_reg(offset - io_base, size);
    return true;
}


bool ATIRage::pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
    uint32_t io_base;

    if (!this->io_access_allowed(offset, &io_base)) {
        return false;
    }

    this->write_reg(offset - io_base, value, size);
    return true;
}


uint32_t ATIRage::read(uint32_t reg_start, uint32_t offset, int size)
{
    LOG_F(8, "Reading ATI Rage PCI memory: region=%X, offset=%X, size %d", reg_start, offset, size);

    if (reg_start < this->aperture_base || offset > APERTURE_SIZE) {
        LOG_F(WARNING, "ATI Rage: attempt to read outside the aperture!");
        return 0;
    }

    if (offset < this->vram_size) {
        /* read from little-endian VRAM region */
        return read_mem(this->vram_ptr + offset, size);
    }
    else if (offset >= MEMMAP_OFFSET) {
        /* read from memory-mapped registers */
        return this->read_reg(offset - MEMMAP_OFFSET, size);
    }
    else {
        LOG_F(WARNING, "ATI Rage: read attempt from unmapped aperture region at 0x%08X", offset);
    }

    return 0;
}

void ATIRage::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    LOG_F(8, "Writing reg=%X, offset=%X, value=%X, size %d", reg_start, offset, value, size);

    if (reg_start < this->aperture_base || offset > APERTURE_SIZE) {
        LOG_F(WARNING, "ATI Rage: attempt to write outside the aperture!");
        return;
    }

    if (offset < this->vram_size) {
        /* write to little-endian VRAM region */
        write_mem(this->vram_ptr + offset, value, size);
    } else if (offset >= MEMMAP_OFFSET) {
        /* write to memory-mapped registers */
        this->write_reg(offset - MEMMAP_OFFSET, value, size);
    }
    else {
        LOG_F(WARNING, "ATI Rage: write attempt to unmapped aperture region at 0x%08X", offset);
    }
}
