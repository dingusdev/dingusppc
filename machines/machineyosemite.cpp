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

/** @file Construct the Yosemite machine (Power Macintosh G3 Blue & White). */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/pci/dec21154.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

static void setup_ram_slot(std::string name, int i2c_addr, int capacity_megs) {
    if (!capacity_megs)
        return;

    gMachineObj->add_device(name, std::unique_ptr<SpdSdram168>(new SpdSdram168(i2c_addr)));
    SpdSdram168* ram_dimm = dynamic_cast<SpdSdram168*>(gMachineObj->get_comp_by_name(name));
    ram_dimm->set_capacity(capacity_megs);

    // register RAM DIMM with the I2C bus
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(i2c_addr, ram_dimm);
}

int initialize_yosemite(std::string& id)
{
    LOG_F(INFO, "Building machine Yosemite...");

    // get pointer to the memory controller/primary PCI bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("Grackle"));

    // get pointer to the bridge of the secondary PCI bus
    DecPciBridge *sec_bridge = dynamic_cast<DecPciBridge*>(gMachineObj->get_comp_by_name("Dec21154"));

    // attach PCI devices to the PCI bridges
    grackle_obj->pci_register_device(DEV_FUN(16,0),
        dynamic_cast<PCIBase*>(gMachineObj->get_comp_by_name("AtiRage128")));

    grackle_obj->pci_register_device(DEV_FUN(13,0),
        dynamic_cast<PCIBase*>(gMachineObj->get_comp_by_name("Dec21154")));

    // register CMD646U2 PCI Ultra ATA Controller
    sec_bridge->pci_register_device(DEV_FUN(1,0),
        dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("CmdAta")));

    sec_bridge->pci_register_device(DEV_FUN(5,0),
        dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Heathrow")));

    InterruptCtrl *int_ctrl_obj =
        dynamic_cast<InterruptCtrl*>(gMachineObj->get_comp_by_type(HWCompType::INT_CTRL));

    static const std::vector<PciIrqMap> grackle_irq_map = {
        {"pci_J12", DEV_FUN(0x10,0), 1 << 22} // GPU PCI slot, 66 MHz
    };

    grackle_obj->set_interrupt_controller(int_ctrl_obj);
    grackle_obj->set_irq_map(grackle_irq_map);

    // 33 MHz PCI devices behind the DEC21154 PCI-to-PCI bridge
    static const std::vector<PciIrqMap> pci_bridge_irq_map = {
        {"pci_FireWire", DEV_FUN(0x00,0), 1 << 21},
        {"pci_UltraATA", DEV_FUN(0x01,0), 1 << 26},
        {"pci_J11",      DEV_FUN(0x02,0), 1 << 23},
        {"pci_J10",      DEV_FUN(0x03,0), 1 << 24},
        {"pci_J9",       DEV_FUN(0x04,0), 1 << 25},
        {"pci_USB",      DEV_FUN(0x06,0), 1 << 28}
    };

    sec_bridge->set_interrupt_controller(int_ctrl_obj);
    sec_bridge->set_irq_map(pci_bridge_irq_map);

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFF00000, 0x100000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM slots
    setup_ram_slot("RAM_DIMM_1", 0x50, GET_INT_PROP("rambank1_size"));
    setup_ram_slot("RAM_DIMM_2", 0x51, GET_INT_PROP("rambank2_size"));
    setup_ram_slot("RAM_DIMM_3", 0x52, GET_INT_PROP("rambank3_size"));
    setup_ram_slot("RAM_DIMM_4", 0x53, GET_INT_PROP("rambank4_size"));

    // configure CPU clocks
    uint64_t bus_freq      = 66820000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // initialize virtual CPU and request MPC750 CPU aka G3
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, false, timebase_freq);

    // set CPU PLL ratio to 3.5
    ppc_state.spr[SPR::HID1] = 0xE << 28;

    return 0;
}

static const PropMap yosemite_settings = {
    {"rambank1_size",
        new IntProperty(256, vector<uint32_t>({8, 16, 32, 64, 128, 256}))},
    {"rambank2_size",
        new IntProperty(  0, vector<uint32_t>({0, 8, 16, 32, 64, 128, 256}))},
    {"rambank3_size",
        new IntProperty(  0, vector<uint32_t>({0, 8, 16, 32, 64, 128, 256}))},
    {"rambank4_size",
        new IntProperty(  0, vector<uint32_t>({0, 8, 16, 32, 64, 128, 256}))},
    {"emmo",
        new BinProperty(0)},
    {"hdd_config",
        new StrProperty("CmdAta0:0")},
    {"cdr_config",
        new StrProperty("Ide0:0")},
};

static vector<string> yosemite_devices = {
    "Grackle", "Dec21154", "CmdAta", "BurgundySnd", "Heathrow", "AtaHardDisk",
    "AtapiCdrom"
};

static const MachineDescription yosemite_descriptor = {
    .name = "pmg3nw",
    .description = "Power Macintosh G3 Blue and White",
    .devices = yosemite_devices,
    .settings = yosemite_settings,
    .init_func = &initialize_yosemite
};

REGISTER_MACHINE(pmg3nw, yosemite_descriptor);
