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

#include <devices/common/pci/pcicardbusbridge.h>
#include <memaccess.h>

typedef struct {
    // CardBus Registers
    /* 0x00 */ uint32_t Event;
    /* 0x04 */ uint32_t Mask;
    /* 0x08 */ uint32_t PresentState;
    /* 0x0C */ uint32_t Force;
    /* 0x10 */ uint32_t Control;
    /* 0x14 */ uint32_t Reserved[4];
    /* 0x20 */ uint32_t UserDefined20h[504];

    // 16-Bit Registers
    /* 0x800 */ uint8_t  IDAndRevision;
    /* 0x801 */ uint8_t  IFStatus;
    /* 0x802 */ uint8_t  PowerAndResetDrvControl;
    /* 0x803 */ uint8_t  InterruptAndGeneralControl;
    /* 0x804 */ uint8_t  CardStatusChange;
    /* 0x805 */ uint8_t  CardStatusChangeInterruptConfiguration;
    /* 0x806 */ uint8_t  AddressWindowEnable;
    /* 0x807 */ uint8_t  IOControl;
    /* 0x808 */ uint16_t IOAdd0Start;
    /* 0x80A */ uint16_t IOAdd0Stop;
    /* 0x80C */ uint16_t IOAdd1Start;
    /* 0x80E */ uint16_t IOAdd1Stop;
    /* 0x810 */ uint16_t SysMemAdd0Start;
    /* 0x812 */ uint16_t SysMemAdd0Stop;
    /* 0x814 */ uint16_t CardMemoryOffsetAdd0;
    /* 0x816 */ uint16_t UserDefined814h;
    /* 0x818 */ uint16_t SysMemAdd1Start;
    /* 0x81A */ uint16_t SysMemAdd1Stop;
    /* 0x81C */ uint16_t CardMemoryOffsetAdd1;
    /* 0x81E */ uint16_t UserDefined81Ch;
    /* 0x820 */ uint16_t SysMemAdd2Start;
    /* 0x822 */ uint16_t SysMemAdd2Stop;
    /* 0x824 */ uint16_t CardMemoryOffsetAdd2;
    /* 0x826 */ uint16_t UserDefined824h;
    /* 0x828 */ uint16_t SysMemAdd3Start;
    /* 0x82A */ uint16_t SysMemAdd3Stop;
    /* 0x82C */ uint16_t CardMemoryOffsetAdd3;
    /* 0x82E */ uint16_t UserDefined82Ch;
    /* 0x830 */ uint16_t SysMemAdd4Start;
    /* 0x832 */ uint16_t SysMemAdd4Stop;
    /* 0x834 */ uint16_t CardMemoryOffsetAdd4;
    /* 0x836 */ uint16_t UserDefined834h;
    /* 0x838 */ uint32_t UserDefined838h[4];
    /* 0x840 */ uint8_t  SysMemAdd0MappingStartUp;
    /* 0x841 */ uint8_t  SysMemAdd1MappingStartUp;
    /* 0x842 */ uint8_t  SysMemAdd2MappingStartUp;
    /* 0x843 */ uint8_t  SysMemAdd3MappingStartUp;
    /* 0x844 */ uint8_t  SysMemAdd4MappingStartUp;
    /* 0x845 */ uint8_t  UserDefined845h;
    /* 0x846 */ uint16_t UserDefined846h;
    /* 0x848 */ uint32_t UserDefined848h[494];
} CardBusStatusAnfControlRegisters;

PCICardbusBridge::PCICardbusBridge(std::string name) : PCIBridgeBase(name, PCI_HEADER_TYPE_2, 1)
{
    this->pci_rd_memory_base_0  = [this]() { return this->memory_base_0  ; };
    this->pci_rd_memory_limit_0 = [this]() { return this->memory_limit_0 ; };
    this->pci_rd_memory_base_1  = [this]() { return this->memory_base_1  ; };
    this->pci_rd_memory_limit_1 = [this]() { return this->memory_limit_1 ; };
    this->pci_rd_io_base_0      = [this]() { return this->io_base_0      ; };
    this->pci_rd_io_limit_0     = [this]() { return this->io_limit_0     ; };
    this->pci_rd_io_base_1      = [this]() { return this->io_base_1      ; };
    this->pci_rd_io_limit_1     = [this]() { return this->io_limit_1     ; };

    this->pci_wr_memory_base_0 = [this](uint32_t val) {
        this->memory_base_0    = val & this->memory_0_cfg;
        this->memory_base_0_32 = this->memory_base_0;
    };

    this->pci_wr_memory_limit_0 = [this](uint32_t val) {
        this->memory_limit_0    = val & this->memory_0_cfg;
        this->memory_limit_0_32 = this->memory_limit_0 + 0x1000;
    };

    this->pci_wr_memory_base_1 = [this](uint32_t val) {
        this->memory_base_1    = val & this->memory_1_cfg;
        this->memory_base_1_32 = this->memory_base_1;
    };

    this->pci_wr_memory_limit_1 = [this](uint32_t val) {
        this->memory_limit_1    = val & this->memory_1_cfg;
        this->memory_limit_1_32 = this->memory_limit_1 + 0x1000;
    };

    this->pci_wr_io_base_0 = [this](uint32_t val) {
        this->io_base_0    = (val & this->io_0_cfg) | (io_0_cfg & 3);
        this->io_base_0_32 = (this->io_base_0 & ~3);
    };

    this->pci_wr_io_limit_0 = [this](uint32_t val) {
        this->io_limit_0    = (val & this->io_0_cfg);
        this->io_limit_0_32 = this->io_limit_0 + 4;
    };

    this->pci_wr_io_base_1 = [this](uint32_t val) {
        this->io_base_1    = (val & this->io_1_cfg) | (io_1_cfg & 3);
        this->io_base_1_32 = (this->io_base_1 & ~3);
    };

    this->pci_wr_io_limit_1 = [this](uint32_t val) {
        this->io_limit_1    = (val & this->io_1_cfg);
        this->io_limit_1_32 = this->io_limit_1 + 4;
    };
};

uint32_t PCICardbusBridge::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_BAR0:
        return this->bars[(reg_offs - 0x10) >> 2];
    case PCI_CFG_CB_CAPABILITIES:
        return (this->pci_rd_sec_status() << 16) | cap_ptr;
    case PCI_CFG_CB_MEMORY_BASE_0:
        return this->pci_rd_memory_base_0();
    case PCI_CFG_CB_MEMORY_LIMIT_0:
        return this->pci_rd_memory_limit_0();
    case PCI_CFG_CB_MEMORY_BASE_1:
        return this->pci_rd_memory_base_0();
    case PCI_CFG_CB_MEMORY_LIMIT_1:
        return this->pci_rd_memory_limit_0();
    case PCI_CFG_CB_IO_BASE_0:
        return this->pci_rd_io_base_0();
    case PCI_CFG_CB_IO_LIMIT_0:
        return this->pci_rd_io_limit_0();
    case PCI_CFG_CB_IO_BASE_1:
        return this->pci_rd_io_base_0();
    case PCI_CFG_CB_IO_LIMIT_1:
        return this->pci_rd_io_limit_0();
    case PCI_CFG_CB_SUBSYSTEM_IDS:
        return (this->subsys_id << 16) | (this->subsys_vndr);
    case PCI_CFG_CB_LEGACY_MODE_BASE:
        return this->legacy_mode_base;
    default:
        return PCIBridgeBase::pci_cfg_read(reg_offs, details);
    }
}

void PCICardbusBridge::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_BAR0:
        this->set_bar_value((reg_offs - 0x10) >> 2, value);
        break;
    case PCI_CFG_CB_CAPABILITIES:
        this->pci_wr_sec_status(value >> 16);
        break;
    case PCI_CFG_CB_MEMORY_BASE_0:
        this->pci_wr_memory_base_0(value);
        break;
    case PCI_CFG_CB_MEMORY_LIMIT_0:
        this->pci_wr_memory_limit_0(value);
        break;
    case PCI_CFG_CB_MEMORY_BASE_1:
        this->pci_wr_memory_base_0(value);
        break;
    case PCI_CFG_CB_MEMORY_LIMIT_1:
        this->pci_wr_memory_limit_0(value);
        break;
    case PCI_CFG_CB_IO_BASE_0:
        this->pci_wr_io_base_0(value);
        break;
    case PCI_CFG_CB_IO_LIMIT_0:
        this->pci_wr_io_limit_0(value);
        break;
    case PCI_CFG_CB_IO_BASE_1:
        this->pci_wr_io_base_0(value);
        break;
    case PCI_CFG_CB_IO_LIMIT_1:
        this->pci_wr_io_limit_0(value);
        break;
/*
    case PCI_CFG_CB_LEGACY_MODE_BASE:
        this->legacy_mode_base = value;
        break;
*/
    default:
        PCIBridgeBase::pci_cfg_write(reg_offs, value, details);
        break;
    }
}

bool PCICardbusBridge::pci_io_read(uint32_t offset, uint32_t size, uint32_t* res)
{
    if (!(this->command & 1)) return false;
    if ((offset < this->io_base_0_32 || offset + size >= this->io_limit_0_32) &&
        (offset < this->io_base_1_32 || offset + size >= this->io_limit_1_32)
    ) return false;
    return this->pci_io_read_loop(offset, size, *res);
}

bool PCICardbusBridge::pci_io_write(uint32_t offset, uint32_t value, uint32_t size)
{
    if (!(this->command & 1)) return false;
    if ((offset < this->io_base_0_32 || offset + size >= this->io_limit_0_32) &&
        (offset < this->io_base_1_32 || offset + size >= this->io_limit_1_32)
    ) return false;
    return this->pci_io_read_loop(offset, size, value);
}
