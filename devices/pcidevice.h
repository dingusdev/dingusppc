#ifndef PCI_DEVICE_H
#define PCI_DEVICE_H

#include <cinttypes>
#include <string>
#include "mmiodevice.h"
#include "pcihost.h"

/* convert little-endian DWORD to big-endian DWORD */
#define LE2BE(x) (x >> 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8) | (x << 24)

enum {
    CFG_REG_BAR0 = 0x10 // base address register 0
};


class PCIDevice : public MMIODevice {
public:
    PCIDevice(std::string name) {this->name = name;};
    virtual ~PCIDevice() = default;

    /* configuration space access methods */
    virtual uint32_t pci_cfg_read(uint32_t reg_offs, uint32_t size) = 0;
    virtual void     pci_cfg_write(uint32_t reg_offs, uint32_t value, uint32_t size) = 0;

    virtual void set_host(PCIHost *host_instance) = 0;

protected:
    std::string   name; // human-readable device name
    PCIHost *host_instance; // host bridge instance to call back
    uint32_t base_addr; // base address register 0
};

#endif /* PCI_DEVICE_H */
