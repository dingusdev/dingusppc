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

#ifndef PCI_BASE_H
#define PCI_BASE_H

#include <devices/common/mmiodevice.h>
#include <devices/common/pci/pcihost.h>

#include <cinttypes>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/** PCI configuration space header types */
enum PCIHeaderType : uint8_t {
    PCI_HEADER_TYPE_0 = 0, // PCI Device
    PCI_HEADER_TYPE_1 = 1, // PCI-PCI Bridge
    PCI_HEADER_TYPE_2 = 2, // PCI-To-Cardbus Bridge
};

/** PCI configuration space registers offsets */
enum {
    PCI_CFG_DEV_ID              = 0x00, // device and vendor IDs
    PCI_CFG_STAT_CMD            = 0x04, // command/status register
    PCI_CFG_CLASS_REV           = 0x08, // class code and revision ID
    PCI_CFG_DWORD_3             = 0x0C, // BIST, HeaderType, Lat_Timer and Cache_Line_Size
    PCI_CFG_BAR0                = 0x10, // base address register 0 (type 0, 1, and 2)
    PCI_CFG_BAR1                = 0x14, // base address register 1 (type 0 and 1)
    PCI_CFG_DWORD_13            = 0x34, // capabilities pointer (type 0 and 1)
    PCI_CFG_DWORD_15            = 0x3C, // Max_Lat, Min_Gnt (Type 0); Bridge Control (Type 1 and 2); Int_Pin and Int_Line registers (type 0, 1, and 2)
};

/** PCI Vendor IDs for devices used in Power Macintosh computers. */
enum {
    PCI_VENDOR_ATI              = 0x1002,
    PCI_VENDOR_DEC              = 0x1011,
    PCI_VENDOR_MOTOROLA         = 0x1057,
    PCI_VENDOR_APPLE            = 0x106B,
    PCI_VENDOR_SILICON_IMAGE    = 0x1095,
    PCI_VENDOR_NVIDIA           = 0x10DE,
};

/** PCI BAR types */
enum PCIBarType : uint32_t {
    Unused = 0,
    Io_16_Bit,
    Io_32_Bit,
    Mem_20_Bit, // legacy type for < 1MB memory
    Mem_32_Bit,
    Mem_64_Bit_Lo,
    Mem_64_Bit_Hi
};

typedef struct {
    uint32_t    bar_num;
    uint32_t    bar_cfg;
} BarConfig;

class PCIBase : public MMIODevice {
public:
    PCIBase(std::string name, PCIHeaderType hdr_type, int num_bars);
    virtual ~PCIBase() = default;

    virtual bool supports_io_space() {
        return has_io_space;
    };

    /* I/O space access methods */
    virtual bool pci_io_read(uint32_t offset, uint32_t size, uint32_t* res) {
        return false;
    };

    virtual bool pci_io_write(uint32_t offset, uint32_t value, uint32_t size) {
        return false;
    };

    // configuration space access methods
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, AccessDetails &details);
    virtual void pci_cfg_write(uint32_t reg_offs, uint32_t value, AccessDetails &details);

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

    int attach_exp_rom_image(const std::string img_path);

    virtual void set_host(PCIHost* host_instance) {
        this->host_instance = host_instance;
    }

    virtual void set_multi_function(bool is_multi_function) {
        this->hdr_type = is_multi_function ? (this->hdr_type | 0x80) : (this->hdr_type & 0x7f);
    }

    virtual void set_irq_pin(uint8_t irq_pin) {
        this->irq_pin = irq_pin;
    }

    virtual void pci_interrupt(uint8_t irq_line_state) {
        this->host_instance->pci_interrupt(irq_line_state, this);
    }

    // MMIODevice methods
    virtual uint32_t read(uint32_t rgn_start, uint32_t offset, int size) { return 0; }
    virtual void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) { }

protected:
    void set_bar_value(int bar_num, uint32_t value);
    void setup_bars(std::vector<BarConfig> cfg_data);
    void finish_config_bars();
    void pci_wr_exp_rom_bar(uint32_t data);
    void map_exp_rom_mem();
    void unmap_exp_rom_mem();

    std::string pci_name;      // human-readable device name
    PCIHost*    host_instance; // host bridge instance to call back

    // PCI configuration space state (type 0, 1, and 2)
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint16_t    command = 0;
    uint16_t    status = 0;
    uint32_t    class_rev;           // class code and revision id
    uint8_t     cache_ln_sz = 0;     // cache line size
    uint8_t     lat_timer = 0;       // latency timer
    uint8_t     hdr_type;            // header type, single function
    uint8_t     cap_ptr = 0;         // capabilities ptr

    uint8_t     irq_pin = 0;
    uint8_t     irq_line = 0;

    bool        has_io_space = false;
    int         num_bars;            // number of BARs. Type 0:6, type 1:2, type 2:1 (4K)
    uint32_t    bars[6] = { 0 };     // BARs (base address registers)
    uint32_t    bars_cfg[6] = { 0 }; // configuration values for BARs
    PCIBarType  bars_typ[6] = { PCIBarType::Unused }; // types for BARs

    // PCI configuration space state (type 0 and 1)
    uint32_t    exp_bar_cfg  = 0;    // expansion ROM configuration
    uint32_t    exp_rom_bar  = 0;    // expansion ROM base address register
    uint32_t    exp_rom_addr = 0;    // expansion ROM base address
    uint32_t    exp_rom_size = 0;    // expansion ROM size in bytes

    std::unique_ptr<uint8_t[]> exp_rom_data;

    // 0 = not writable; 1 = bit is enabled in command register
    uint16_t    command_cfg = 0xffff - (1<<3) - (1<<7); // disable: special cycles and stepping
};

inline uint32_t pci_cfg_log(uint32_t value, AccessDetails &details) {
    switch (details.size << 2 | details.offset) {
        case 0x04: return (uint8_t) value;
        case 0x05: return (uint8_t)(value >> 8);
        case 0x06: return (uint8_t)(value >> 16);
        case 0x07: return (uint8_t)(value >> 24);

        case 0x08: return (uint16_t)  value;
        case 0x09: return (uint16_t) (value >>  8);
        case 0x0a: return (uint16_t) (value >> 16);
        case 0x0b: return (uint16_t)((value >> 24) | (value << 8));

        case 0x10: return  value;
        case 0x11: return (value >>  8) | (value << 24);
        case 0x12: return (value >> 16) | (value << 16);
        case 0x13: return (value >> 24) | (value <<  8);

        default: return 0xffffffff;
    }
}

#define LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER() \
    do { if ((details.flags & PCI_CONFIG_DIRECTION) == PCI_CONFIG_READ) { \
        VLOG_F( \
            (~-details.size & details.offset) ? loguru::Verbosity_ERROR : loguru::Verbosity_WARNING, \
            "%s: read  unimplemented config register @%02x.%c", \
            this->name.c_str(), reg_offs + details.offset, \
            SIZE_ARG(details.size) \
        ); \
    } } while(0)

#define LOG_NAMED_CONFIG_REGISTER(reg_verb, reg_name) \
    VLOG_F( \
        (~-details.size & details.offset) ? loguru::Verbosity_ERROR : loguru::Verbosity_WARNING, \
        "%s: %s %s register @%02x.%c = %0*x", \
        this->name.c_str(), reg_verb, reg_name, reg_offs + details.offset, \
        SIZE_ARG(details.size), \
        details.size * 2, pci_cfg_log(value, details) \
    )

#define LOG_READ_NAMED_CONFIG_REGISTER(reg_name) \
    do { if ((details.flags & PCI_CONFIG_DIRECTION) == PCI_CONFIG_READ) { \
        LOG_NAMED_CONFIG_REGISTER("read ", reg_name); \
    } } while(0)

#define LOG_WRITE_NAMED_CONFIG_REGISTER(reg_name) \
    LOG_NAMED_CONFIG_REGISTER("write", reg_name)

#define LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER_WITH_VALUE() \
    LOG_READ_NAMED_CONFIG_REGISTER("unimplemented config")

#define LOG_WRITE_UNIMPLEMENTED_CONFIG_REGISTER() \
    LOG_WRITE_NAMED_CONFIG_REGISTER("unimplemented config")

#define LOG_READ_NON_EXISTENT_PCI_DEVICE() \
    LOG_F( \
        ERROR, \
        "%s err: read attempt from non-existent PCI device %02x:%02x.%x @%02x.%c", \
        this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + details.offset, \
        SIZE_ARG(details.size) \
    )

#define LOG_WRITE_NON_EXISTENT_PCI_DEVICE() \
    LOG_F( \
        ERROR, \
        "%s err: write attempt to non-existent PCI device %02x:%02x.%x @%02x.%c = %0*x", \
        this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + details.offset, \
        SIZE_ARG(details.size), \
        details.size * 2, BYTESWAP_SIZED(value, details.size) \
    )

#endif /* PCI_BASE_H */
