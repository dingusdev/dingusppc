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

#ifndef PCI_CARDBUSBRIDGE_H
#define PCI_CARDBUSBRIDGE_H

#include <devices/deviceregistry.h>
#include <devices/common/pci/pcibridgebase.h>
#include <devices/common/pci/pcihost.h>

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <vector>

/** PCI configuration space registers offsets */
enum {
    PCI_CFG_CB_CAPABILITIES     = 0x14, // CB_CAPABILITIES.b, 0.b, CB_SEC_STATUS.w
    PCI_CFG_CB_MEMORY_BASE_0    = 0x1C,
    PCI_CFG_CB_MEMORY_LIMIT_0   = 0x20,
    PCI_CFG_CB_MEMORY_BASE_1    = 0x24,
    PCI_CFG_CB_MEMORY_LIMIT_1   = 0x28,
    PCI_CFG_CB_IO_BASE_0        = 0x2C,
    PCI_CFG_CB_IO_LIMIT_0       = 0x30,
    PCI_CFG_CB_IO_BASE_1        = 0x34,
    PCI_CFG_CB_IO_LIMIT_1       = 0x38,
    PCI_CFG_CB_SUBSYSTEM_IDS    = 0x40, // CB_SUBSYSTEM_VENDOR_ID.w, CB_SUBSYSTEM_ID.w
    PCI_CFG_CB_LEGACY_MODE_BASE = 0x44,
};

class PCICardbusBridge : public PCIBridgeBase {
public:
    PCICardbusBridge(std::string name);
    ~PCICardbusBridge() = default;

    // PCIBase methods
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    virtual void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

    virtual bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    virtual bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

    // plugin interface for using in the derived classes
    std::function<uint32_t()>        pci_rd_memory_base_0;
    std::function<void(uint32_t)>    pci_wr_memory_base_0;
    std::function<uint32_t()>        pci_rd_memory_limit_0;
    std::function<void(uint32_t)>    pci_wr_memory_limit_0;
    std::function<uint32_t()>        pci_rd_memory_base_1;
    std::function<void(uint32_t)>    pci_wr_memory_base_1;
    std::function<uint32_t()>        pci_rd_memory_limit_1;
    std::function<void(uint32_t)>    pci_wr_memory_limit_1;
    std::function<uint32_t()>        pci_rd_io_base_0;
    std::function<void(uint32_t)>    pci_wr_io_base_0;
    std::function<uint32_t()>        pci_rd_io_limit_0;
    std::function<void(uint32_t)>    pci_wr_io_limit_0;
    std::function<uint32_t()>        pci_rd_io_base_1;
    std::function<void(uint32_t)>    pci_wr_io_base_1;
    std::function<uint32_t()>        pci_rd_io_limit_1;
    std::function<void(uint32_t)>    pci_wr_io_limit_1;

protected:
    // PCI configuration space state
    uint32_t    memory_base_0 = 0;
    uint32_t    memory_limit_0 = 0;
    uint32_t    memory_base_1 = 0;
    uint32_t    memory_limit_1 = 0;
    uint32_t    io_base_0 = 0;
    uint32_t    io_limit_0 = 0;
    uint32_t    io_base_1 = 0;
    uint32_t    io_limit_1 = 0;
    uint16_t    subsys_id = 0;
    uint16_t    subsys_vndr = 0;
    uint32_t    legacy_mode_base = 0;

    // 0 = not writable
    uint32_t    memory_0_cfg = 0xfffff000;
    uint32_t    memory_1_cfg = 0xfffff000;

    // 0 = not writable, 0x0000fffc = supports 16 bit I/O range, 0xfffffffd = supports 32 bit I/O range
    uint32_t    io_0_cfg = 0x0000fffc;
    uint32_t    io_1_cfg = 0x0000fffc;

    // calculated address ranges
    uint32_t    memory_base_0_32 = 0;
    uint32_t    memory_limit_0_32 = 0;
    uint32_t    memory_base_1_32 = 0;
    uint32_t    memory_limit_1_32 = 0;
    uint32_t    io_base_0_32 = 0;
    uint32_t    io_limit_0_32 = 0;
    uint32_t    io_base_1_32 = 0;
    uint32_t    io_limit_1_32 = 0;
};

#endif /* PCI_CARDBUSBRIDGE_H */
