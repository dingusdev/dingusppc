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

#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcibridge.h>
#include <devices/common/pci/pcihost.h>
#include <devices/memctrl/memctrlbase.h>
#include <machines/machinebase.h>
#include <endianswap.h>
#include <loguru.hpp>

#include <cinttypes>

bool PCIHost::pci_register_device(int dev_fun_num, PCIBase* dev_instance)
{
    // return false if dev_fun_num already registered
    if (this->dev_map.count(dev_fun_num))
        return false;

    int fun_num = dev_fun_num & 7;
    int dev_num = (dev_fun_num >> 3) & 0x1f;
    bool is_multi_function = fun_num != 0;

    for (int other_fun_num = 0; other_fun_num < 8; other_fun_num++) {
        if (this->dev_map.count(DEV_FUN(dev_num, other_fun_num))) {
            is_multi_function = true;
            if (is_multi_function && other_fun_num == 0) {
                this->dev_map[DEV_FUN(dev_num, other_fun_num)]->set_multi_function(true);
            }
        }
    }

    this->dev_map[dev_fun_num] = dev_instance;

    dev_instance->set_host(this);
    if (is_multi_function && fun_num == 0) {
        dev_instance->set_multi_function(true);
    }

    if (dev_instance->supports_io_space()) {
        this->io_space_devs.push_back(dev_instance);
    }

    PCIBridgeBase *bridge = dynamic_cast<PCIBridgeBase*>(dev_instance);
    if (bridge) {
        this->bridge_devs.push_back(bridge);
    }

    return true;
}

bool PCIHost::pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj)
{
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    // FIXME: add sanity checks!
    return mem_ctrl->add_mmio_region(start_addr, size, obj);
}

bool PCIHost::pci_unregister_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj)
{
    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
                           (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));
    // FIXME: add sanity checks!
    return mem_ctrl->remove_mmio_region(start_addr, size, obj);
}

void PCIHost::attach_pci_device(const std::string& dev_name, int slot_id)
{
    this->attach_pci_device(dev_name, slot_id, "");
}

PCIBase *PCIHost::attach_pci_device(const std::string& dev_name, int slot_id, const std::string& dev_suffix)
{
    if (!DeviceRegistry::device_registered(dev_name)) {
        HWComponent *hwc = dynamic_cast<HWComponent*>(this);
        LOG_F(
            WARNING, "%s: specified PCI device %s doesn't exist",
            hwc ? hwc->get_name().c_str() : "PCIHost", dev_name.c_str()
        );
        return NULL;
    }

    // attempt to create device object
    auto dev_obj = DeviceRegistry::get_descriptor(dev_name).m_create_func();

    if (!dev_obj->supports_type(HWCompType::PCI_DEV)) {
        HWComponent *hwc = dynamic_cast<HWComponent*>(this);
        LOG_F(
            WARNING, "%s: cannot attach non-PCI device %s",
            hwc ? hwc->get_name().c_str() : "PCIHost", dev_name.c_str()
        );

        return NULL;
    }

    // add device to the machine object
    gMachineObj->add_device(dev_name + dev_suffix, std::move(dev_obj));

    PCIBase *dev = dynamic_cast<PCIBase*>(gMachineObj->get_comp_by_name(dev_name + dev_suffix));

    // register device with the PCI host
    this->pci_register_device(slot_id, dev);

    return dev;
}

bool PCIHost::pci_io_read_loop(uint32_t offset, int size, uint32_t &res)
{
    for (auto& dev : this->io_space_devs) {
        if (dev->pci_io_read(offset, size, &res)) {
            return true;
        }
    }
    return false;
}

bool PCIHost::pci_io_write_loop(uint32_t offset, int size, uint32_t value)
{
    for (auto& dev : this->io_space_devs) {
        if (dev->pci_io_write(offset, value, size)) {
            return true;
        }
    }
    return false;
}

uint32_t PCIHost::pci_io_read_broadcast(uint32_t offset, int size)
{
    uint32_t res;
    // broadcast I/O request to devices that support I/O space
    // until a device returns true that means "request accepted"
    if (pci_io_read_loop (offset, size, res)) {
        return res;
    }
    HWComponent *hwc = dynamic_cast<HWComponent*>(this);
    LOG_F(
        ERROR, "%s: Attempt to read from unmapped PCI I/O space @%08x.%c",
        hwc ? hwc->get_name().c_str() : "PCIHost", offset,
        size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size
    );
    // FIXME: add machine check exception (DEFAULT CATCH!, code=FFF00200)
    return 0;
}

void PCIHost::pci_io_write_broadcast(uint32_t offset, int size, uint32_t value)
{
    // broadcast I/O request to devices that support I/O space
    // until a device returns true that means "request accepted"
    if (pci_io_write_loop(offset, size, value)) {
        return;
    }
    HWComponent *hwc = dynamic_cast<HWComponent*>(this);
    LOG_F(
        ERROR, "%s: Attempt to write to unmapped PCI I/O space @%08x.%c = %0*x",
        hwc ? hwc->get_name().c_str() : "PCIHost", offset,
        size == 4 ? 'l' : size == 2 ? 'w' : size == 1 ? 'b' : '0' + size,
        size * 2, BYTESWAP_SIZED(value, size)
    );
}

PCIBase *PCIHost::pci_find_device(uint8_t bus_num, uint8_t dev_num, uint8_t fun_num)
{
    for (auto& bridge : this->bridge_devs) {
        if (bridge->secondary_bus <= bus_num) {
            if (bridge->secondary_bus == bus_num) {
                if (bridge->dev_map.count(DEV_FUN(dev_num, fun_num))) {
                    return bridge->dev_map[DEV_FUN(dev_num, fun_num)];
                }
            }
            else if (bridge->subordinate_bus >= bus_num) {
                return bridge->pci_find_device(bus_num, dev_num, fun_num);
            }
        }
    }
    return NULL;
}
