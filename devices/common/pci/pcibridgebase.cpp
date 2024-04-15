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

#include <devices/common/pci/pcibridgebase.h>
#include <memaccess.h>

PCIBridgeBase::PCIBridgeBase(std::string name, PCIHeaderType hdr_type, int num_bars) : PCIBase(name, hdr_type, num_bars)
{
    this->pci_rd_primary_bus        = [this]() { return this->primary_bus; };
    this->pci_rd_secondary_bus      = [this]() { return this->secondary_bus; };
    this->pci_rd_subordinate_bus    = [this]() { return this->subordinate_bus; };
    this->pci_rd_sec_latency_timer  = [this]() { return this->sec_latency_timer; };
    this->pci_rd_sec_status         = [this]() { return this->sec_status; };
    this->pci_rd_bridge_control     = [this]() { return this->bridge_control; };

    this->pci_wr_primary_bus        = [this](uint8_t  val) { this->primary_bus = val; };
    this->pci_wr_secondary_bus      = [this](uint8_t  val) { this->secondary_bus = val; };
    this->pci_wr_subordinate_bus    = [this](uint8_t  val) { this->subordinate_bus = val; };
    this->pci_wr_sec_latency_timer  = [this](uint8_t  val) {
        this->sec_latency_timer = (this->sec_latency_timer & ~this->sec_latency_timer_cfg) |
                                  (val & this->sec_latency_timer_cfg);
    };
    this->pci_wr_sec_status         = [this](uint16_t val) {};
    this->pci_wr_bridge_control     = [this](uint16_t val) { this->bridge_control = val; };
};

bool PCIBridgeBase::pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj)
{
    // FIXME: constrain region to memory range
    return this->host_instance->pci_register_mmio_region(start_addr, size, obj);
}

bool PCIBridgeBase::pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj)
{
    return this->host_instance->pci_unregister_mmio_region(start_addr, size, obj);
}

uint32_t PCIBridgeBase::pci_cfg_read(uint32_t reg_offs, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_PRIMARY_BUS:
        return (this->pci_rd_sec_latency_timer() << 24) |
               (this->pci_rd_subordinate_bus() << 16)   |
               (this->pci_rd_secondary_bus() << 8)      |
               (this->pci_rd_primary_bus());
    case PCI_CFG_DWORD_15:
        return (this->pci_rd_bridge_control() << 16) | (irq_pin << 8) | irq_line;
    default:
        return PCIBase::pci_cfg_read(reg_offs, details);
    }
}

void PCIBridgeBase::pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details)
{
    switch (reg_offs) {
    case PCI_CFG_PRIMARY_BUS:
        this->pci_wr_sec_latency_timer(value >> 24);
        this->pci_wr_subordinate_bus(value >> 16);
        this->pci_wr_secondary_bus(value >> 8);
        this->pci_wr_primary_bus(value & 0xFFU);
        break;
    case PCI_CFG_DWORD_15:
        this->irq_line = value >> 24;
        this->pci_wr_bridge_control(value >> 16);
        break;
    default:
        return PCIBase::pci_cfg_write(reg_offs, value, details);
    }
}

int PCIBridgeBase::device_postinit() {
    return this->pcihost_device_postinit();
}
