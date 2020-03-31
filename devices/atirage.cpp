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
#include "endianswap.h"
#include "memreadwrite.h"
#include "pcidevice.h"
#include <thirdparty/loguru/loguru.hpp>

ATIRage::ATIRage(uint16_t dev_id) : PCIDevice("ati-rage")
{
    WRITE_DWORD_BE_A(&this->pci_cfg[0], (dev_id << 16) | ATI_PCI_VENDOR_ID);
    WRITE_DWORD_BE_A(&this->pci_cfg[8], 0x0300005C);
    WRITE_DWORD_BE_A(&this->pci_cfg[0x3C], 0x00080100);
}

void ATIRage::write_reg(uint32_t offset, uint32_t value, uint32_t size)
{
    /* size-dependent endian convertsion */
    /* FIXME: make it reusable */
    switch (size) {
    case 4:
        value = BYTESWAP_32(value);
        break;
    case 2:
        value = BYTESWAP_16(value);
        break;
    }

    switch (offset) {
    case ATI_CRTC_INT_CNTL:
        LOG_F(INFO, "ATI Rage: CRTC_INT_CNTL set to 0x%X", value);
        break;
    case ATI_CRTC_GEN_CNTL:
        LOG_F(INFO, "ATI Rage: CRTC_GEN_CNTL set to 0x%X", value);
        break;
    case ATI_MEM_ADDR_CFG:
        LOG_F(INFO, "ATI Rage: MEM_ADDR_CFG set to 0x%X", value);
        break;
    case ATI_BUS_CNTL:
        LOG_F(INFO, "ATI Rage: BUS_CNTL set to 0x%X", value);
        break;
    case ATI_EXT_MEM_CNTL:
        LOG_F(INFO, "ATI Rage: EXT_MEM_CNTL set to 0x%X", value);
        break;
    case ATI_MEM_CNTL:
        LOG_F(INFO, "ATI Rage: MEM_CNTL set to 0x%X", value);
        break;
    case ATI_DAC_CNTL:
        LOG_F(INFO, "ATI Rage: DAC_CNTL set to 0x%X", value);
        break;
    case ATI_GEN_TEST_CNTL:
        LOG_F(INFO, "ATI Rage: GEN_TEST_CNTL set to 0x%X", value);
        break;
    case ATI_CFG_STAT0:
        LOG_F(INFO, "ATI Rage: CONFIG_STAT0 set to 0x%X", value);
        break;
    default:
        LOG_F(ERROR, "ATI Rage: unknown register at 0x%X", offset);
    }
}


uint32_t ATIRage::pci_cfg_read(uint32_t reg_offs, uint32_t size)
{
    uint32_t res = 0;

    LOG_F(INFO, "Reading ATI Rage config space, offset = 0x%X, size=%d", reg_offs, size);

    switch (size) {
    case 4:
        res = READ_DWORD_LE_U(&this->pci_cfg[reg_offs]);
        break;
    case 2:
        res = READ_WORD_LE_U(&this->pci_cfg[reg_offs]);
        break;
    case 1:
        res = this->pci_cfg[reg_offs];
        break;
    default:
        LOG_F(WARNING, "ATI Rage pci_cfg_read(): invalid size %d", size);
    }

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
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR0], 0xFF000008);
        }
        else {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR0], value);
        }
        break;
    case 0x14: /* BAR 1: I/O space base, 256 bytes wide */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR1], 0x0000FFF1);
        }
        else {
            WRITE_DWORD_BE_A(&this->pci_cfg[CFG_REG_BAR1], value);
        }
    case 0x18: /* BAR 2 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR2], 0xFFFFF000);
        }
        else {
            WRITE_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR2], value);
        }
        break;
    case CFG_REG_BAR3: /* unimplemented */
    case CFG_REG_BAR4: /* unimplemented */
    case CFG_REG_BAR5: /* unimplemented */
    case CFG_EXP_BASE: /* no expansion ROM */
        WRITE_DWORD_BE_A(&this->pci_cfg[reg_offs], 0);
        break;
    default:
        WRITE_DWORD_LE_A(&this->pci_cfg[reg_offs], value);
    }
}


bool ATIRage::pci_io_read(uint32_t offset, uint32_t size, uint32_t *res)
{
    LOG_F(INFO, "ATI Rage I/O space read, offset=0x%X, size=%d", offset, size);
    return false;
}


bool ATIRage::pci_io_write(uint32_t offset, uint32_t value, uint32_t size)
{
    uint32_t io_base = READ_DWORD_LE_A(&this->pci_cfg[CFG_REG_BAR1]) & ~3;

    if (!(this->pci_cfg[CFG_REG_CMD] & 1)) {
        LOG_F(WARNING, "ATI I/O space disabled in the command reg");
        return false;
    }
    if (offset < io_base || offset >(io_base + 0x100)) {
        LOG_F(WARNING, "Rage: I/O out of range, base=0x%X, offset=0x%X", io_base, offset);
        return false;
    }

    this->write_reg(offset - io_base, value, size);
    return true;
}


uint32_t ATIRage::read(uint32_t reg_start, uint32_t offset, int size)
{
    LOG_F(INFO, "Reading reg=%X, size %d", offset, size);
    return 0;
}

void ATIRage::write(uint32_t reg_start, uint32_t offset, uint32_t value, int size)
{
    LOG_F(INFO, "Writing reg=%X, value=%X, size %d", offset, value, size);
}
