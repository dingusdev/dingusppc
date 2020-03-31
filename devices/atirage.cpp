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
            WRITE_DWORD_LE_A(&this->pci_cfg[0x10], 0xFF000008);
        }
        else {
            WRITE_DWORD_LE_A(&this->pci_cfg[0x10], value);
        }
        break;
    case 0x14: /* BAR 1: I/O space base, 256 bytes wide */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[0x14], 0x0000FFF1);
        }
        else {
            WRITE_DWORD_LE_A(&this->pci_cfg[0x14], value);
        }
    case 0x18: /* BAR 2 */
        if (value == 0xFFFFFFFFUL) {
            WRITE_DWORD_LE_A(&this->pci_cfg[0x18], 0xFFFFF000);
        }
        else {
            WRITE_DWORD_LE_A(&this->pci_cfg[0x18], value);
        }
        break;
    case 0x1C: /* BAR 3: unimplemented */
    case 0x20: /* BAR 4: unimplemented */
    case 0x24: /* BAR 5: unimplemented */
    case 0x30: /* Expansion ROM Base Addr: unimplemented */
        WRITE_DWORD_LE_A(&this->pci_cfg[reg_offs], 0);
        break;
    default:
        WRITE_DWORD_LE_A(&this->pci_cfg[reg_offs], value);
    }
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
