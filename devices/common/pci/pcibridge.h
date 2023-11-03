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

#ifndef PCI_BRIDGE_H
#define PCI_BRIDGE_H

#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>

#include <cinttypes>
#include <string>

/** PCI configuration space registers offsets */
enum {
    PCI_CFG_PRIMARY_BUS         = 0x18, // PRIMARY_BUS, SECONDARY_BUS, SUBORDINATE_BUS, SEC_LATENCY_TIMER
    PCI_CFG_IO_BASE             = 0x1C, // IO_BASE.b, IO_LIMIT.b, SEC_STATUS.w
    PCI_CFG_MEMORY_BASE         = 0x20, // MEMORY_BASE.w, MEMORY_LIMIT.w
    PCI_CFG_PREF_MEM_BASE       = 0x24, // PREF_MEMORY_BASE.w, PREF_MEMORY_LIMIT.w
    PCI_CFG_PREF_BASE_UPPER32   = 0x28, // PREF_BASE_UPPER32
    PCI_CFG_PREF_LIMIT_UPPER32  = 0x2c, // PREF_LIMIT_UPPER32
    PCI_CFG_IO_BASE_UPPER16     = 0x30, // IO_BASE_UPPER16.w, IO_LIMIT_UPPER16.w
    // PCI_CFG_CAP_PTR
    PCI_CFG_BRIDGE_ROM_ADDRESS  = 0x38, // BRIDGE_ROM_ADDRESS
    PCI_CFG_INTERRUPT_LINE      = 0x3C, // INTERRUPT_LINE.b, INTERRUPT_PIN.b, BRIDGE_CONTROL.w
};

class PCIBridge : public PCIHost, public PCIDevice {
    friend class PCIHost;
public:
    PCIBridge(std::string name);
    ~PCIBridge() = default;

    // PCIHost methods
    virtual bool pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIDevice* obj);
    virtual bool pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIDevice* obj);

    // PCIDevice methods
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    virtual void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

    virtual bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res);
    virtual bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size);

    // MMIODevice methods
    virtual uint32_t read(uint32_t rgn_start, uint32_t offset, int size) {
        return 0;
    };
    virtual void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) { };

    // plugin interface for using in the derived classes
    std::function<uint8_t()>        pci_rd_primary_bus;
    std::function<void(uint8_t)>    pci_wr_primary_bus;
    std::function<uint8_t()>        pci_rd_secondary_bus;
    std::function<void(uint8_t)>    pci_wr_secondary_bus;
    std::function<uint8_t()>        pci_rd_subordinate_bus;
    std::function<void(uint8_t)>    pci_wr_subordinate_bus;
    std::function<uint8_t()>        pci_rd_sec_latency_timer;
    std::function<void(uint8_t)>    pci_wr_sec_latency_timer;
    std::function<uint16_t()>       pci_rd_sec_status;
    std::function<void(uint16_t)>   pci_wr_sec_status;
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
    std::function<uint16_t()>       pci_rd_bridge_control;
    std::function<void(uint16_t)>   pci_wr_bridge_control;

protected:
    // PCI configuration space state
    uint8_t     primary_bus = 0;
    uint8_t     secondary_bus = 0;
    uint8_t     subordinate_bus = 0;
    uint8_t     sec_latency_timer = 0; // if supportss r/w then must reset to 0
    uint16_t    sec_status = 0;
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
    uint16_t    bridge_control = 0;

    // 0 = not writable, 0xf8 = limits the granularity to eight PCI clocks
    uint8_t     sec_latency_timer_cfg = 0;

    // 0 = not writable, 0xf0 = supports 16 bit io range, 0xf1 = supports 32 bit I/O range
    uint8_t     io_cfg = 0xf0;

    // 0 = not writable, 0xfff0 = supports 32 bit memory range
    uint16_t    memory_cfg = 0xfff0;

    // 0 = not writable, 0xfff0 = supports 32 bit prefetchable memory range,
    // 0xfff1 = supports 64 bit prefetchable memory range
    uint16_t    pref_mem_cfg = 0xfff0;

    uint32_t    io_base_32 = 0;
    uint32_t    io_limit_32 = 0;
    uint64_t    memory_base_32 = 0;
    uint64_t    memory_limit_32 = 0;
    uint64_t    pref_mem_base_64 = 0;
    uint64_t    pref_mem_limit_64 = 0;

private:
};

#endif /* PCI_BRIDGE_H */
