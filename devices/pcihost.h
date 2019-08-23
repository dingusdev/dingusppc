#ifndef PCI_HOST_H
#define PCI_HOST_H

#include <cinttypes>

class PCIDevice; // forward declaration to prevent errors

class PCIHost {
public:
    virtual bool pci_register_device(int dev_num, PCIDevice *dev_instance) = 0;
    virtual bool pci_register_mmio_region(uint32_t start_addr, uint32_t size,
        PCIDevice *obj) = 0;
};

#endif /* PCI_HOST_H */
