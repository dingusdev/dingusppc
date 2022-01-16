#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/memctrl/memctrlbase.h>
#include <machines/machinebase.h>

#include <cinttypes>

bool PCIHost::pci_register_device(int dev_num, PCIDevice* dev_instance)
{
    // return false if dev_num already registered
    if (this->dev_map.count(dev_num))
        return false;

    this->dev_map[dev_num] = dev_instance;

    dev_instance->set_host(this);

    if (dev_instance->supports_io_space()) {
        this->io_space_devs.push_back(dev_instance);
    }

    return true;
}

bool PCIHost::pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIDevice* obj)
{
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    // FIXME: add sanity checks!
    return mem_ctrl->add_mmio_region(start_addr, size, obj);
}
