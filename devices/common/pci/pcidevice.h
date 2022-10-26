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
#include <memory>
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
    PCI_CFG_CAP_PTR   = 0x34, // capabilities pointer
    PCI_CFG_DWORD_14  = 0x38, // reserved
    PCI_CFG_DWORD_15  = 0x3C, // Max_Lat, Min_Gnt, Int_Pin and Int_Line registers
};

/** PCI Vendor IDs for devices used in Power Macintosh computers. */
enum {
    PCI_VENDOR_ATI      = 0x1002,
    PCI_VENDOR_MOTOROLA = 0x1057,
    PCI_VENDOR_APPLE    = 0x106B,
    PCI_VENDOR_NVIDIA   = 0x10DE,
};

/** PCI BAR types */
enum PCIBarType {
    BAR_Unused,
    BAR_IO_16Bit,
    BAR_IO_32Bit,
    BAR_MEM_20Bit, // < 1M
    BAR_MEM_32Bit,
    BAR_MEM_64Bit,
    BAR_MEM_64BitHi,
};

class PCIDevice : public MMIODevice {
public:
    PCIDevice(std::string name);
    virtual ~PCIDevice() = default;

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
    };
    
    virtual void set_multi_function(bool is_multi_function) {
        this->hdr_type = is_multi_function ? (this->hdr_type | 0x80) : (this->hdr_type & 0x7f);
    }
    virtual void set_irq_pin(uint8_t irq_pin) {
        this->irq_pin = irq_pin;
    }

    // MMIODevice methods
    virtual uint32_t read(uint32_t rgn_start, uint32_t offset, int size) { return 0; }
    virtual void write(uint32_t rgn_start, uint32_t offset, uint32_t value, int size) { }

protected:
    void do_bar_sizing(int bar_num);
    void set_bar_value(int bar_num, uint32_t value);
    void finish_config_bars();
    void pci_wr_exp_rom_bar(uint32_t data);
    void map_exp_rom_mem();
    void unmap_exp_rom_mem();

    std::string pci_name;      // human-readable device name
    PCIHost* host_instance;    // host bridge instance to call back

    // PCI configuration space state
    uint16_t    vendor_id;
    uint16_t    device_id;
    uint32_t    class_rev;       // class code and revision id
    uint16_t    status = 0;
    uint16_t    command = 0;
    uint8_t     hdr_type = 0;    // header type, single function
    uint8_t     lat_timer = 0;   // latency timer
    uint8_t     cache_ln_sz = 0; // cache line size
    uint16_t    subsys_id = 0;
    uint16_t    subsys_vndr = 0;
    uint8_t     cap_ptr = 0;
    uint8_t     max_lat = 0;
    uint8_t     min_gnt = 0;
    uint8_t     irq_pin = 0;
    uint8_t     irq_line = 0;

    bool        has_io_space = false;
    int         num_bars = 6;
    uint32_t    bars[6] = { 0 };     // base address registers
    uint32_t    bars_cfg[6] = { 0 }; // configuration values for base address registers
    PCIBarType  bars_typ[6] = { BAR_Unused }; // types for base address registers

    uint32_t    exp_bar_cfg  = 0;    // expansion ROM configuration
    uint32_t    exp_rom_bar  = 0;    // expansion ROM base address register
    uint32_t    exp_rom_addr = 0;    // expansion ROM base address
    uint32_t    exp_rom_size = 0;    // expansion ROM size in bytes

    std::unique_ptr<uint8_t[]> exp_rom_data;
};

/* value is dword from PCI config. MSB..LSB of value is stored in PCI config as 0:LSB..3:MSB.
   result is part of value at byte offset from LSB with size bytes (with wrap around) and flipped as required for pci_cfg_read result. */
inline uint32_t pci_cfg_rev_read(uint32_t value, AccessDetails &details) {
    switch (details.size << 2 | details.offset) {
        case 0x04: return  value        & 0xff; // 0
        case 0x05: return (value >>  8) & 0xff; // 1
        case 0x06: return (value >> 16) & 0xff; // 2
        case 0x07: return (value >> 24) & 0xff; // 3

        case 0x08: return ((value & 0xff) << 8)    | ((value >>  8) & 0xff); // 0 1
        case 0x09: return ( value        & 0xff00) | ((value >> 16) & 0xff); // 1 2
        case 0x0a: return ((value >>  8) & 0xff00) | ((value >> 24) & 0xff); // 2 3
        case 0x0b: return ((value >> 16) & 0xff00) | ( value        & 0xff); // 3 0

        case 0x10: return ((value &       0xff) << 24) | ((value &  0xff00) <<  8) | ((value >>  8) & 0xff00) | ((value >> 24) & 0xff); // 0 1 2 3
        case 0x11: return ((value &     0xff00) << 16) | ( value       & 0xff0000) | ((value >> 16) & 0xff00) | ( value        & 0xff); // 1 2 3 0
        case 0x12: return ((value &   0xff0000) <<  8) | ((value >> 8) & 0xff0000) | ((value & 0xff) << 8)    | ((value >>  8) & 0xff); // 2 3 0 1
        case 0x13: return ( value & 0xff000000)        | ((value &    0xff) << 16) | ( value        & 0xff00) | ((value >> 16) & 0xff); // 3 0 1 2

        default: return 0xffffffff;
    }
}

/* value is dword from PCI config. MSB..LSB of value (3.2.1.0) is stored in PCI config as 0:LSB..3:MSB.
   newvalue is flipped bytes (d0.d1.d2.d3, as passed to pci_cfg_write) to be merged into value.
   result is part of value at byte offset from LSB with size bytes (with wrap around) modified by newvalue. */
inline uint32_t pci_cfg_rev_write(uint32_t value, AccessDetails &details, uint32_t newvalue) {
    switch (details.size << 2 | details.offset) {
        case 0x04: return (value & 0xffffff00) |  (newvalue & 0xff);        //  3  2  1 d0
        case 0x05: return (value & 0xffff00ff) | ((newvalue & 0xff) <<  8); //  3  2 d0  0
        case 0x06: return (value & 0xff00ffff) | ((newvalue & 0xff) << 16); //  3 d0  1  0
        case 0x07: return (value & 0x00ffffff) | ((newvalue & 0xff) << 24); // d0  2  1  0

        case 0x08: return (value & 0xffff0000) | ((newvalue >> 8) & 0xff)    | ((newvalue & 0xff) <<  8); //  3  2 d1 d0
        case 0x09: return (value & 0xff0000ff) |  (newvalue & 0xff00)        | ((newvalue & 0xff) << 16); //  3 d1 d0  0
        case 0x0a: return (value & 0x0000ffff) | ((newvalue & 0xff00) <<  8) | ((newvalue & 0xff) << 24); // d1 d0  1  0
        case 0x0b: return (value & 0x00ffff00) | ((newvalue & 0xff00) << 16) |  (newvalue & 0xff);        // d0  2  1 d1

        case 0x10: return ((newvalue &       0xff) << 24) | ((newvalue &   0xff00) <<  8) | ((newvalue >>  8) & 0xff00) | ((newvalue >> 24) & 0xff); // d3 d2 d1 d0
        case 0x11: return ((newvalue &     0xff00) << 16) | ( newvalue        & 0xff0000) | ((newvalue >> 16) & 0xff00) | ( newvalue        & 0xff); // d2 d1 d0 d3
        case 0x12: return ((newvalue &   0xff0000) <<  8) | ((newvalue >> 8)  & 0xff0000) | ((newvalue & 0xff) << 8)    | ((newvalue >>  8) & 0xff); // d1 d0 d3 d2
        case 0x13: return ( newvalue & 0xff000000)        | ((newvalue &     0xff) << 16) | ( newvalue        & 0xff00) | ((newvalue >> 16) & 0xff); // d0 d3 d2 d1

        default: return 0xffffffff;
    }
}

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

#define SIZE_ARGS details.size == 4 ? 'l' : details.size == 2 ? 'w' : details.size == 1 ? 'b' : '0' + details.size

#define LOG_READ_UNIMPLEMENTED_CONFIG_REGISTER() \
    do { if ((details.flags & PCI_CONFIG_DIRECTION) == PCI_CONFIG_READ) { \
        VLOG_F( \
            (~-details.size & details.offset) ? loguru::Verbosity_ERROR : loguru::Verbosity_WARNING, \
            "%s: read  unimplemented config register @%02x.%c", \
            this->name.c_str(), reg_offs + details.offset, \
            SIZE_ARGS \
        ); \
    } } while(0)

#define LOG_NAMED_CONFIG_REGISTER(reg_verb, reg_name) \
    VLOG_F( \
        (~-details.size & details.offset) ? loguru::Verbosity_ERROR : loguru::Verbosity_WARNING, \
        "%s: %s %s register @%02x.%c = %0*x", \
        this->name.c_str(), reg_verb, reg_name, reg_offs + details.offset, \
        SIZE_ARGS, \
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
        "%s err: read  attempt from non-existent PCI device %02x:%02x.%x @%02x.%c", \
        this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + details.offset, \
        SIZE_ARGS \
    )

#define LOG_WRITE_NON_EXISTENT_PCI_DEVICE() \
    LOG_F( \
        ERROR, \
        "%s err: write attempt  to  non-existent PCI device %02x:%02x.%x @%02x.%c = %0*x", \
        this->name.c_str(), bus_num, dev_num, fun_num, reg_offs + details.offset, \
        SIZE_ARGS, \
        details.size * 2, BYTESWAP_SIZED(value, details.size) \
    )

#endif /* PCI_DEVICE_H */
