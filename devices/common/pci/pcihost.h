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

#ifndef PCI_HOST_H
#define PCI_HOST_H

#include <core/bitops.h>
#include <devices/common/hwinterrupt.h>
#include <endianswap.h>

#include <cinttypes>
#include <string>
#include <unordered_map>
#include <vector>

enum {
    PCI_CONFIG_DIRECTION    = 1,
    PCI_CONFIG_READ         = 0,
    PCI_CONFIG_WRITE        = 1,

    PCI_CONFIG_TYPE         = 4,
    PCI_CONFIG_TYPE_0       = 0,
    PCI_CONFIG_TYPE_1       = 4,
}; // PCIAccessFlags

/** PCI config space access details */
typedef struct AccessDetails {
    uint8_t size;
    uint8_t offset;
    uint8_t flags;
} AccessDetails;

#define DEV_FUN(dev_num,fun_num) (((dev_num) << 3) | (fun_num))

typedef struct {
    const char *    slot_name;
    int             dev_fun_num;
    IntSrc          int_src;
} PciIrqMap;

class PCIBase;
class PCIBridgeBase;

class PCIHost {
public:
    PCIHost() {
        this->dev_map.clear();
        io_space_devs.clear();
    };
    ~PCIHost() = default;

    virtual bool pci_register_device(int dev_fun_num, PCIBase* dev_instance);
    virtual void pci_unregister_device(int dev_fun_num);

    virtual bool pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj);
    virtual bool pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj);

    virtual void attach_pci_device(const std::string& dev_name, int slot_id);
    PCIBase *attach_pci_device(const std::string& dev_name, int slot_id,
                               const std::string& dev_suffix);

    virtual bool pci_io_read_loop (uint32_t offset, int size, uint32_t &res);
    virtual bool pci_io_write_loop(uint32_t offset, int size, uint32_t value);

    virtual uint32_t pci_io_read_broadcast (uint32_t offset, int size);
    virtual void     pci_io_write_broadcast(uint32_t offset, int size, uint32_t value);

    virtual PCIBase *pci_find_device(uint8_t bus_num, uint8_t dev_num, uint8_t fun_num);
    virtual PCIBase *pci_find_device(uint8_t dev_num, uint8_t fun_num);

    virtual void set_irq_map(const std::vector<PciIrqMap> &irq_map) {
        this->my_irq_map = irq_map;
    };
    virtual int pcihost_device_postinit();

    virtual bool register_pci_int(PCIBase* dev_instance);
    virtual void set_interrupt_controller(InterruptCtrl * int_ctrl_obj) {
        this->int_ctrl = int_ctrl_obj;
    };
    virtual InterruptCtrl *get_interrupt_controller();

protected:
    std::unordered_map<int, PCIBase*> dev_map;
    std::vector<PCIBase*>             io_space_devs;
    std::vector<PCIBridgeBase*>       bridge_devs;
    std::vector<PciIrqMap>            my_irq_map;

    InterruptCtrl   *int_ctrl = nullptr;
};

// Helpers for data conversion in the PCI Configuration space.

/**
    Perform size dependent endian swapping for value that is dword from PCI config or any other dword little endian register.

    Unaligned data is handled properly by using bytes from the next dword.
 */
inline uint32_t pci_conv_rd_data(uint32_t value, uint32_t value2, AccessDetails &details) {
    switch (details.size << 2 | details.offset) {
    // Bytes
    case 0x04:
        return value & 0xFF;            // 0
    case 0x05:
        return (value >>  8) & 0xFF;    // 1
    case 0x06:
        return (value >> 16) & 0xFF;    // 2
    case 0x07:
        return (value >> 24) & 0xFF;    // 3

    // Words
    case 0x08:
        return BYTESWAP_16(value);                          // 0 1
    case 0x09:
        return BYTESWAP_16((value >>  8) & 0xFFFFU);        // 1 2
    case 0x0A:
        return BYTESWAP_16((value >> 16) & 0xFFFFU);        // 2 3
    case 0x0B:
        return ((value >> 16) & 0xFF00) | (value2 & 0xFF);  // 3 4

    // Dwords
    case 0x10:
        return BYTESWAP_32(value);                          // 0 1 2 3
    case 0x11:
        value = (uint32_t)((((uint64_t)value2 << 32) | value) >>  8);
        return BYTESWAP_32(value);                          // 1 2 3 4
    case 0x12:
        value = (uint32_t)((((uint64_t)value2 << 32) | value) >> 16);
        return BYTESWAP_32(value);                          // 2 3 4 5
    case 0x13:
        value = (uint32_t)((((uint64_t)value2 << 32) | value) >> 24);
        return BYTESWAP_32(value);                          // 3 4 5 6
    default:
        return 0xFFFFFFFFUL;
    }
}

/**
    Perform size dependent endian swapping for v2, then merge v2 with v1 under
    control of a mask generated according with the size parameter.

    Unaligned data is handled properly by wrapping around if needed.
 */
inline uint32_t pci_conv_wr_data(uint32_t v1, uint32_t v2, AccessDetails &details)
{
    switch (details.size << 2 | details.offset) {
    // Bytes
    case 0x04:
        return (v1 & ~0xFF)      |  (v2 & 0xFF);        //  3  2  1 d0
    case 0x05:
        return (v1 & ~0xFF00)    | ((v2 & 0xFF) << 8);  //  3  2 d0  0
    case 0x06:
        return (v1 & ~0xFF0000)  | ((v2 & 0xFF) << 16); //  3 d0  1  0
    case 0x07:
        return (v1 & 0x00FFFFFF) | ((v2 & 0xFF) << 24); // d0  2  1  0

    // Words
    case 0x08:
        return (v1 & ~0xFFFF)    |  BYTESWAP_16(v2);        //  3  2 d1 d0
    case 0x09:
        return (v1 & ~0xFFFF00)  | (BYTESWAP_16(v2) << 8);  //  3 d1 d0  0
    case 0x0a:
        return (v1 & 0x0000FFFF) | (BYTESWAP_16(v2) << 16); // d1 d0  1  0
    case 0x0b:
        return (v1 & 0x00FFFF00) | ((v2 & 0xFF00) << 16) |
               (v2 & 0xFF);                                 // d0  2  1 d1

    // Dwords
    case 0x10:
        return BYTESWAP_32(v2);              // d3 d2 d1 d0
    case 0x11:
        return ROTL_32(BYTESWAP_32(v2), 8);  // d2 d1 d0 d3
    case 0x12:
        return ROTL_32(BYTESWAP_32(v2), 16); // d1 d0 d3 d2
    case 0x13:
        return ROTR_32(BYTESWAP_32(v2), 8);  // d0 d3 d2 d1

    default:
        return 0xFFFFFFFFUL;
    }
}

#endif /* PCI_HOST_H */
