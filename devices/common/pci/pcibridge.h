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

#ifndef PCI_BRIDGE_H
#define PCI_BRIDGE_H

#include <devices/common/pci/pcibridgebase.h>
#include <devices/common/pci/pcihost.h>

#include <cinttypes>
#include <string>

/** PCI configuration space registers offsets */
enum {
    PCI_CFG_IO_BASE             = 0x1C, // IO_BASE.b, IO_LIMIT.b, SEC_STATUS.w
    PCI_CFG_MEMORY_BASE         = 0x20, // MEMORY_BASE.w, MEMORY_LIMIT.w
    PCI_CFG_PREF_MEM_BASE       = 0x24, // PREF_MEMORY_BASE.w, PREF_MEMORY_LIMIT.w
    PCI_CFG_PREF_BASE_UPPER32   = 0x28, // PREF_BASE_UPPER32
    PCI_CFG_PREF_LIMIT_UPPER32  = 0x2c, // PREF_LIMIT_UPPER32
    PCI_CFG_IO_BASE_UPPER16     = 0x30, // IO_BASE_UPPER16.w, IO_LIMIT_UPPER16.w
    PCI_CFG_BRIDGE_ROM_ADDRESS  = 0x38, // BRIDGE_ROM_ADDRESS
};

class PCIBridge : public PCIBridgeBase {
public:
    PCIBridge(std::string name);
    ~PCIBridge() = default;

    // PCIBase methods
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    virtual void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

    virtual bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    virtual bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

    // plugin interface for using in the derived classes
    std::function<uint8_t()>        pci_rd_io_base;
    std::function<void(uint8_t)>    pci_wr_io_base;
    std::function<uint8_t()>        pci_rd_io_limit;
    std::function<void(uint8_t)>    pci_wr_io_limit;
    std::function<uint16_t()>       pci_rd_memory_base;
    std::function<void(uint16_t)>   pci_wr_memory_base;
    std::function<uint16_t()>       pci_rd_memory_limit;
    std::function<void(uint16_t)>   pci_wr_memory_limit;
    std::function<uint16_t()>       pci_rd_pref_mem_base;
    std::function<void(uint16_t)>   pci_wr_pref_mem_base;
    std::function<uint16_t()>       pci_rd_pref_mem_limit;
    std::function<void(uint16_t)>   pci_wr_pref_mem_limit;
    std::function<uint32_t()>       pci_rd_pref_base_upper32;
    std::function<void(uint32_t)>   pci_wr_pref_base_upper32;
    std::function<uint32_t()>       pci_rd_pref_limit_upper32;
    std::function<void(uint32_t)>   pci_wr_pref_limit_upper32;
    std::function<uint16_t()>       pci_rd_io_base_upper16;
    std::function<void(uint16_t)>   pci_wr_io_base_upper16;
    std::function<uint16_t()>       pci_rd_io_limit_upper16;
    std::function<void(uint16_t)>   pci_wr_io_limit_upper16;

protected:
    // PCI configuration space state
    uint8_t     io_base = 0;
    uint8_t     io_limit = 0;
    uint16_t    memory_base = 0;
    uint16_t    memory_limit = 0;
    uint16_t    pref_mem_base = 0;
    uint16_t    pref_mem_limit = 0;
    uint32_t    pref_base_upper32 = 0;
    uint32_t    pref_limit_upper32 = 0;
    uint16_t    io_base_upper16 = 0;
    uint16_t    io_limit_upper16 = 0;

    // 0 = not writable, 0xf0 = supports 16 bit I/O range, 0xf1 = supports 32 bit I/O range
    uint8_t     io_cfg = 0xf0;

    // 0 = not writable, 0xfff0 = supports 32 bit memory range
    uint16_t    memory_cfg = 0xfff0;

    // 0 = not writable, 0xfff0 = supports 32 bit prefetchable memory range,
    // 0xfff1 = supports 64 bit prefetchable memory range
    uint16_t    pref_mem_cfg = 0xfff0;

    // calculated address ranges
    uint32_t    io_base_32 = 0;
    uint32_t    io_limit_32 = 0;
    uint64_t    memory_base_32 = 0;
    uint64_t    memory_limit_32 = 0;
    uint64_t    pref_mem_base_64 = 0;
    uint64_t    pref_mem_limit_64 = 0;
};

#endif /* PCI_BRIDGE_H */
