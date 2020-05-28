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

#include <atirage.h>
#include <cstdint>
#include <thirdparty/loguru/loguru.hpp>
#include "endianswap.h"
#include "memreadwrite.h"
#include "pcidevice.h"
#include "displayid.h"


ATIRage::ATIRage(uint16_t dev_id) : PCIDevice("ati-rage")
{
    WRITE_DWORD_BE_A(&this->pci_cfg[0], (dev_id << 16) | ATI_PCI_VENDOR_ID);
    WRITE_DWORD_BE_A(&this->pci_cfg[8], 0x0300005C);
    WRITE_DWORD_BE_A(&this->pci_cfg[0x3C], 0x00080100);

    this->disp_id = new DisplayID();
}

ATIRage::~ATIRage()
{
    delete (this->disp_id);
}

uint32_t ATIRage::size_dep_read(uint8_t *buf, uint32_t size)
{
    switch (size) {
    case 4:
        return READ_DWORD_LE_A(buf);
        break;
    case 2:
        return READ_WORD_LE_A(buf);
        break;
    case 1:
        return *buf;
        break;
    default:
        LOG_F(WARNING, "ATI Rage read: invalid size %d", size);
        return 0;
    }
}

void ATIRage::size_dep_write(uint8_t *buf, uint32_t value, uint32_t size)
{
    switch (size) {
    case 4:
        WRITE_DWORD_BE_A(buf, value);
        break;
    case 2:
        WRITE_WORD_BE_A(buf, value & 0xFFFFU);
        break;
    case 1:
        *buf = value & 0xFF;
    }
}

const char* ATIRage::get_reg_name(uint32_t reg_offset)
{
    const char* reg_name;

    switch (reg_offset & ~3) {
    case ATI_CRTC_H_TOTAL_DISP:
        reg_name = "CRTC_H_TOTAL_DISP";
        break;
    case ATI_CRTC_H_SYNC_STRT_WID:
        reg_name = "CRTC_H_SYNC_STRT_WID";
        break;
    case ATI_CRTC_V_TOTAL_DISP:
        reg_name = "CRTC_V_TOTAL_DISP";
        break;
    case ATI_CRTC_V_SYNC_STRT_WID:
        reg_name = "CRTC_V_SYNC_STRT_WID";
        break;
    case ATI_CRTC_OFF_PITCH:
        reg_name = "CRTC_OFF_PITCH";
        break;
    case ATI_CRTC_INT_CNTL:
        reg_name = "CRTC_INT_CNTL";
        break;
    case ATI_CRTC_GEN_CNTL:
        reg_name = "CRTC_GEN_CNTL";
        break;
    case ATI_DSP_CONFIG:
        reg_name = "DSP_CONFIG";
        break;
    case ATI_DSP_ON_OFF:
        reg_name = "DSP_ON_OFF";
        break;
    case ATI_MEM_ADDR_CFG:
        reg_name = "MEM_ADDR_CFG";
        break;
    case ATI_OVR_CLR:
        reg_name = "OVR_CLR";
        break;
    case ATI_OVR_WID_LEFT_RIGHT:
        reg_name = "OVR_WID_LEFT_RIGHT";
        break;
    case ATI_OVR_WID_TOP_BOTTOM:
        reg_name = "OVR_WID_TOP_BOTTOM";
        break;
    case ATI_GP_IO:
        reg_name = "GP_IO";
        break;
    case ATI_CLOCK_CNTL:
        reg_name = "CLOCK_CNTL";
        break;
    case ATI_BUS_CNTL:
        reg_name = "BUS_CNTL";
        break;
    case ATI_EXT_MEM_CNTL:
        reg_name = "EXT_MEM_CNTL";
        break;
    case ATI_MEM_CNTL:
        reg_name = "MEM_CNTL";
        break;
    case ATI_DAC_CNTL:
        reg_name = "DAC_CNTL";
        break;
    case ATI_GEN_TEST_CNTL:
        reg_name = "GEN_TEST_CNTL";
        break;
    case ATI_CFG_CHIP_ID:
        reg_name = "CONFIG_CHIP_ID";
        break;
    case ATI_CFG_STAT0:
        reg_name = "CONFIG_STAT0";
        break;
    default:
        reg_name = "unknown";
    }

    return reg_name;
}

uint32_t ATIRage::read_reg(uint32_t offset, uint32_t size)
{
    uint32_t res;

    switch (offset & ~3) {
    case ATI_GP_IO:
        break;
    default:
        LOG_F(INFO, "ATI Rage: read I/O reg %s at 0x%X, size=%d, val=0x%X",
            get_reg_name(offset), offset, size,
            size_dep_read(&this->block_io_regs[offset], size));
    }

    res = size_dep_read(&this->block_io_regs[offset], size);

    return res;
}

void ATIRage::write_reg(uint32_t offset, uint32_t value, uint32_t size)
{
    uint32_t gpio_val;
    uint16_t gpio_dir;

    /* size-dependent endian conversion */
    size_dep_write(&this->block_io_regs[offset], value, size);

    switch (offset & ~3) {
    case ATI_GP_IO:
        if (offset < (ATI_GP_IO + 2)) {
            gpio_val = READ_DWORD_LE_A(&this->block_io_regs[ATI_GP_IO]);
            gpio_dir = (gpio_val >> 16) & 0x3FFF;
            WRITE_WORD_LE_A(&this->block_io_regs[ATI_GP_IO],
                this->disp_id->read_monitor_sense(gpio_val, gpio_dir));
        }
        break;
    default:
        LOG_F(INFO, "ATI Rage: %s register at 0x%X set to 0x%X",
            get_reg_name(offset), offset & ~3,
            READ_DWORD_LE_A(&this->block_io_regs[offset & ~3]));
    }
}


uint32_t ATIRage::pci_cfg_read(uint32_t reg_offs, uint32_t size)
{
    uint32_t res = 0;

    LOG_F(INFO, "Reading ATI Rage config space, offset = 0x%X, size=%d", reg_offs, size);

    res = size_dep_read(&this->pci_cfg[reg_offs], size);

    LOG_F(INFO, "Return value: 0x%X", res);
    return res;
}

void ATIRage::pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size)
{
    LOG_F(INFO, "Writing into ATI Rage PCI config space, offset = 0x%X, val=0x%X size=%d",
        reg_offs, BYTESWAP_32(value), size);

    switch (reg_offs) {
    case 0x10: /* BAR 0 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR0], 0xFF000000UL);
        }
        else {
            this->aperture_base = BYTESWAP_32(value);
            LOG_F(INFO, "ATI Rage aperture address set to 0x%08X", this->aperture_base);
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR0], value);
            this->host_instance->pci_register_mmio_region(this->aperture_base, 0x01000000, this);
        }
        break;
    case 0x14: /* BAR 1: I/O space base, 256 bytes wide */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR1], 0xFFFFFF01UL);
        }
        else {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR1], value);
        }
    case 0x18: /* BAR 2 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR2], 0xFFFFF000UL);
        }
        else {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR2], value);
        }
        break;
    case CFG_REG_BAR3: /* unimplemented */
    case CFG_REG_BAR4: /* unimplemented */
    case CFG_REG_BAR5: /* unimplemented */
    case CFG_EXP_BASE: /* no expansion ROM */
        WRITE_DWORD_LE_A(&this->pci_cfg[reg_offs], 0);
        break;
    default:
        size_dep_write(&this->pci_cfg[reg_offs], value, size);
    }
}


bool ATIRage::io_access_allowed(uint32_t offset, uint32_t *p_io_base)
{
    if (!(this->pci_cfg[CFG_REG_CMD] & 1)) {
        LOG_F(WARNING, "ATI I/O space disabled in the command reg");
        return false;
    }

    uint32_t io_base = READ_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR1]) & ~3;

    if (offset < io_base || offset > (io_base + 0x100)) {
        LOG_F(WARNING, "Rage: I/O out of range, base=0x%X, offset=0x%X", io_base, offset);
        return false;
    }

    *p_io_base = io_base;

    return true;
}


bool ATIRage::pci_io_read(uint32_t offset, uint32_t size, uint32_t *res)
{
    uint32_t io_base;

    if (!this->io_access_allowed(offset, &io_base)) {
        return false;
    }

    *res = this->read_reg(offset - io_base, size);
    return true;
}


bool ATIRage::pci_io_write(uint32_t offset, uint32_t value, uint32_t size)
{
    uint32_t io_base;

    if (!this->io_access_allowed(offset, &io_base)) {
        return false;
    }

    this->write_reg(offset - io_base, value, size);
    return true;
}


uint32_t ATIRage::read(uint32_t reg_start, uint32_t offset, int size)
{
    LOG_F(INFO, "Reading ATI Rage PCI memory: region=%X, offset=%X, size %d", reg_start, offset, size);

    if (reg_start < this->aperture_base || offset > 0x01000000) {
        LOG_F(WARNING, "ATI Rage address out of range!");
        return 0;
    }

    if (offset < 0x7FFC00UL) {
        LOG_F(WARNING, "ATI Rage frame buffer reads not supported yet!");
        return 0;
    }

    return this->read_reg(offset - 0x7FFC00UL, size);
}

void ATIRage::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    LOG_F(INFO, "Writing reg=%X, offset=%X, value=%X, size %d", reg_start, offset, value, size);

    if (reg_start < this->aperture_base || offset > 0x01000000) {
        LOG_F(WARNING, "ATI Rage address out of range!");
        return;
    }

    if (offset < 0x7FFC00UL) {
        LOG_F(WARNING, "ATI Rage frame buffer writes not supported yet!");
        return;
    }

    this->write_reg(offset - 0x7FFC00UL, value, size);
}
