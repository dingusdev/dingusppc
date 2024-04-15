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

#ifndef PCI_BRIDGE_BASE_H
#define PCI_BRIDGE_BASE_H

#include <devices/deviceregistry.h>
#include <devices/common/pci/pcibase.h>
#include <devices/common/pci/pcihost.h>

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <vector>

/** PCI configuration space registers offsets (type 1 and 2) */
enum {
    PCI_CFG_PRIMARY_BUS         = 0x18, // PRIMARY_BUS, SECONDARY_BUS, SUBORDINATE_BUS, SEC_LATENCY_TIMER
};

class PCIBridgeBase : public PCIHost, public PCIBase {
    friend class PCIHost;
public:
    PCIBridgeBase(std::string name, PCIHeaderType hdr_type, int num_bars);
    ~PCIBridgeBase() = default;

    // PCIHost methods
    virtual bool pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj);
    virtual bool pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj);

    // PCIBase methods
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    virtual void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

    bool supports_io_space() {
        return true;
    };

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
    std::function<uint16_t()>       pci_rd_bridge_control;
    std::function<void(uint16_t)>   pci_wr_bridge_control;

    // HWComponent methods
    virtual int device_postinit();

protected:
    // PCI configuration space state
    uint8_t     primary_bus = 0;
    uint8_t     secondary_bus = 0;
    uint8_t     subordinate_bus = 0;
    uint8_t     sec_latency_timer = 0; // if supportss r/w then must reset to 0
    uint16_t    sec_status = 0;
    uint16_t    bridge_control = 0;

    // 0 = not writable, 0xf8 = limits the granularity to eight PCI clocks
    uint8_t     sec_latency_timer_cfg = 0xff;
};

#endif /* PCI_BRIDGE_BASE_H */
