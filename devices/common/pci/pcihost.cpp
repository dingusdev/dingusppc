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

#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/memctrl/memctrlbase.h>
#include <machines/machinebase.h>
#include <loguru.hpp>

#include <cinttypes>

bool PCIHost::pci_register_device(int dev_fun_num, PCIDevice* dev_instance)
{
    // return false if dev_num already registered
    if (this->dev_map.count(dev_fun_num))
        return false;

    this->dev_map[dev_fun_num] = dev_instance;

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

bool PCIHost::pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIDevice* obj)
{
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    // FIXME: add sanity checks!
    return mem_ctrl->remove_mmio_region(start_addr, size, obj);
}

void PCIHost::attach_pci_device(std::string& dev_name, int slot_id)
{
    if (!DeviceRegistry::device_registered(dev_name)) {
        LOG_F(WARNING, "PCIHost: specified PCI device %s doesn't exist", dev_name.c_str());
        return;
    }

    // attempt to create device object
    auto dev_obj = DeviceRegistry::get_descriptor(dev_name).m_create_func();

    if (!dev_obj->supports_type(HWCompType::PCI_DEV)) {
        LOG_F(WARNING, "PCIHost: cannot attach non-PCI device %s", dev_name.c_str());
        return;
    }

    // add device to the machine object
    gMachineObj->add_device(dev_name, std::move(dev_obj));

    // register device with the PCI host
    this->pci_register_device(
        slot_id, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name(dev_name)));
}

PCIDevice *PCIHost::pci_find_device(uint8_t bus_num, uint8_t dev_num, uint8_t fun_num)
{
    return NULL;
}
