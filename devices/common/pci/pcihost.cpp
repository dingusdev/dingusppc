/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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
#include <devices/deviceregistry.h>
#include <machines/machinefactory.h>
#include <machines/machinebase.h>
#include <endianswap.h>
#include <loguru.hpp>

#include <cinttypes>
#include <cpu/ppc/ppcemu.h>

bool PCIHost::pci_register_device(int dev_fun_num, PCIBase* dev_instance)
{
    // return false if dev_fun_num already registered
    if (this->dev_map.count(dev_fun_num)) {
        pci_unregister_device(dev_fun_num);
    }

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

void PCIHost::pci_unregister_device(int dev_fun_num)
{
    if (!this->dev_map.count(dev_fun_num)) {
        return;
    }
    auto dev_instance = this->dev_map[dev_fun_num];

    HWComponent *hwc = dynamic_cast<HWComponent*>(this);
    LOG_F(
        // FIXME: need destructors to remove memory regions and downstream devices.
        ERROR, "%s: pci_unregister_device(%s) not supported yet (every PCI device needs "
            "a working destructor to unregister memory and downstream devices etc.)",
        hwc ? hwc->get_name().c_str() : "PCIHost", dev_instance->get_name().c_str()
    );

    this->dev_map.erase(dev_fun_num);
    gMachineObj->remove_device(dev_instance); // calls destructor of dev_instance since it is a unique_ptr in the device_map.
}

AddressMapEntry* PCIHost::pci_register_mmio_region(uint32_t start_addr, uint32_t size, PCIBase* obj)
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
    MachineFactory::register_device_settings(dev_name);
    auto desc = DeviceRegistry::get_descriptor(dev_name);
    auto dev_obj = desc.m_create_func();

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
    dev->set_name(dev->get_name() + dev_suffix);

    // register device with the PCI host
    this->pci_register_device(slot_id, dev);

    return dev;
}

InterruptCtrl *PCIHost::get_interrupt_controller() {
    if (!this->int_ctrl) {
        InterruptCtrl *int_ctrl_obj =
            dynamic_cast<InterruptCtrl*>(gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));
        this->int_ctrl = int_ctrl_obj;
    }
    return this->int_ctrl;
}

bool PCIHost::register_pci_int(PCIBase* dev_instance) {
    bool dev_found   = false;
    int  dev_fun_num = 0;

    for (auto& dev : this->dev_map) {
        if (dev.second == dev_instance) {
            dev_fun_num = dev.first;
            dev_found = true;
        }
    }

    if (!dev_found)
        ABORT_F("register_pci_int: requested device not found");

    for (auto& irq : this->my_irq_map) {
        if (irq.dev_fun_num == dev_fun_num) {
            IntDetails new_int_detail;
            new_int_detail.int_ctrl_obj = this->get_interrupt_controller();
            if (new_int_detail.int_ctrl_obj && irq.int_src)
                new_int_detail.irq_id = new_int_detail.int_ctrl_obj->register_dev_int(irq.int_src);
            dev_instance->set_int_details(new_int_detail);
            return true;
        }
    }

    PCIBridgeBase *self = dynamic_cast<PCIBridgeBase*>(this);
    if (self) {
        if (!self->int_details.int_ctrl_obj)
            self->host_instance->register_pci_int(self);
        dev_instance->set_int_details(self->int_details);
        return true;
    }
    return false;
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
    if (pci_io_read_loop(offset, size, res))
        return res;

    // no device has accepted the request -> report error
    HWComponent *hwc = dynamic_cast<HWComponent*>(this);
    LOG_F(
        ERROR, "%s: Attempt to read from unmapped PCI I/O space @%08x.%c",
        hwc ? hwc->get_name().c_str() : "PCIHost", offset,
        SIZE_ARG(size)
    );
    // machine check exception (DEFAULT CATCH!, code=FFF00200)
    ppc_exception_handler(Except_Type::EXC_MACHINE_CHECK, 0);
    return 0;
}

void PCIHost::pci_io_write_broadcast(uint32_t offset, int size, uint32_t value)
{
    // broadcast I/O request to devices that support I/O space
    // until a device returns true that means "request accepted"
    if (pci_io_write_loop(offset, size, value))
        return;

    // no device has accepted the request -> report error
    HWComponent *hwc = dynamic_cast<HWComponent*>(this);
    LOG_F(
        ERROR, "%s: Attempt to write to unmapped PCI I/O space @%08x.%c = %0*x",
        hwc ? hwc->get_name().c_str() : "PCIHost", offset,
        SIZE_ARG(size),
        size * 2, BYTESWAP_SIZED(value, size)
    );
}

PCIBase *PCIHost::pci_find_device(uint8_t bus_num, uint8_t dev_num, uint8_t fun_num)
{
    for (auto& bridge : this->bridge_devs) {
        if (bridge->secondary_bus <= bus_num) {
            if (bridge->secondary_bus == bus_num) {
                return bridge->pci_find_device(dev_num, fun_num);
            }
            if (bridge->subordinate_bus >= bus_num) {
                return bridge->pci_find_device(bus_num, dev_num, fun_num);
            }
        }
    }
    return NULL;
}

PCIBase *PCIHost::pci_find_device(uint8_t dev_num, uint8_t fun_num)
{
    if (this->dev_map.count(DEV_FUN(dev_num, fun_num))) {
        return this->dev_map[DEV_FUN(dev_num, fun_num)];
    }
    return NULL;
}

int PCIHost::pcihost_device_postinit()
{
    std::string pci_dev_name;

    for (auto& slot : this->my_irq_map) {
        if (slot.slot_name) {
            pci_dev_name = GET_STR_PROP(slot.slot_name);
            if (!pci_dev_name.empty()) {
                this->attach_pci_device(pci_dev_name, slot.dev_fun_num, std::string("@") + slot.slot_name);
            }
        }
    }

    return 0;
}
