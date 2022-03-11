/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-22 divingkatae and maximum
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

#ifndef PCI_DEVICE_H
#define PCI_DEVICE_H

#include <devices/common/mmiodevice.h>
#include <devices/common/pci/pcihost.h>

#include <cinttypes>
#include <functional>
#include <string>

/** PCI configuration space registers offsets */
enum {
    PCI_CFG_DEV_ID    = 0x00, // device and vendor IDs
    PCI_CFG_STAT_CMD  = 0x04, // command/status register
    PCI_CFG_CLASS_REV = 0x08, // class code and revision ID
    PCI_CFG_DWORD_3   = 0x0C, // BIST, HeaderType, Lat_Timer and Cache_Line_Size
    PCI_CFG_BAR0      = 0x10, // base address register 0
    PCI_CFG_BAR1      = 0x14, // base address register 1
    PCI_CFG_BAR2      = 0x18, // base address register 2
    PCI_CFG_BAR3      = 0x1C, // base address register 3
    PCI_CFG_BAR4      = 0x20, // base address register 4
    PCI_CFG_BAR5      = 0x24, // base address register 5
    PCI_CFG_CIS_PTR   = 0x28, // Cardbus CIS Pointer
    PCI_CFG_SUBSYS_ID = 0x2C, // Subsysten IDs
    PCI_CFG_ROM_BAR   = 0x30, // expansion ROM base address
    PCI_CFG_DWORD_15  = 0x3C, // Max_Lat, Min_Gnt, Int_Pin and Int_Line registers
};

/** PCI Vendor IDs for devices used in Power Macintosh computers. */
enum {
    PCI_VENDOR_ATI      = 0x1002,
    PCI_VENDOR_MOTOROLA = 0x1057,
    PCI_VENDOR_APPLE    = 0x106B,
};


class PCIDevice : public MMIODevice {
public:
    PCIDevice(std::string name);
    virtual ~PCIDevice() = default;

    virtual bool supports_io_space() {
        return false;
    };

    /* I/O space access methods */
    virtual bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
        return false;
    };

    virtual bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
        return false;
    };

    // configuration space access methods
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size);
    virtual void pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size);

    // plugin interface for using in the derived classes
    std::function<uint16_t()>       pci_rd_stat;
    std::function<void(uint16_t)>   pci_wr_stat;
    std::function<uint16_t()>       pci_rd_cmd;
    std::function<void(uint16_t)>   pci_wr_cmd;
    std::function<uint8_t()>        pci_rd_bist;
    std::function<void(uint8_t)>    pci_wr_bist;
    std::function<uint8_t()>        pci_rd_lat_timer;
    std::function<void(uint8_t)>    pci_wr_lat_timer;
    std::function<uint8_t()>        pci_rd_cache_lnsz;
    std::function<void(uint8_t)>    pci_wr_cache_lnsz;

    std::function<void(int)>        pci_notify_bar_change;

    virtual void set_host(PCIHost* host_instance) {
        this->host_instance = host_instance;
    };

protected:
    void do_bar_sizing(int bar_num);
    void set_bar_value(int bar_num, uint32_t value);

    std::string pci_name;      // human-readable device name
    PCIHost* host_instance;    // host bridge instance to call back

    // PCI configuration space state
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint32_t    class_rev;       // class code and revision id
    uint16_t    status = 0;
    uint16_t    command = 0;
    uint8_t     hdr_type = 0;    // header type
    uint8_t     lat_timer = 0;   // latency timer
    uint8_t     cache_ln_sz = 0; // cache line size
    uint16_t    subsys_id = 0;
    uint16_t    subsys_vndr = 0;
    uint8_t     max_lat = 0;
    uint8_t     min_gnt = 0;
    uint8_t     irq_pin = 0;
    uint8_t     irq_line = 0;

    uint32_t    bars[6] = { 0 };     // base address registers
    uint32_t    bars_cfg[6] = { 0 }; // configuration values for base address registers
    uint32_t    exp_rom_bar = 0;     // expansion ROM base address
    uint32_t    exp_bar_cfg = 0;
};

#endif /* PCI_DEVICE_H */
