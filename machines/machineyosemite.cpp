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
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

int initialize_yosemite(std::string& id)
{
    LOG_F(INFO, "Building machine Yosemite...");

    // get pointer to the memory controller/primary PCI bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("Grackle"));

    // get pointer to the bridge of the secondary PCI bus
    DecPciBridge *sec_bridge = dynamic_cast<DecPciBridge*>(gMachineObj->get_comp_by_name("Dec21154"));

    // connect PCI devices
    grackle_obj->pci_register_device(
        13, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Dec21154")));

    sec_bridge->pci_register_device(
        5, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Heathrow")));

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFF00000, 0x100000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure CPU clocks
    uint64_t bus_freq      = 66820000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // initialize virtual CPU and request MPC750 CPU aka G3
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, timebase_freq);

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
    {"emmo",
        new BinProperty(0)},
};

static vector<string> pmg3nw_devices = {
    "Grackle", "Dec21154", "Heathrow"
};

static const MachineDescription pmg3nw_descriptor = {
    .name = "pmg3dt",
    .description = "Power Macintosh G3 Blue and White",
    .devices = pmg3nw_devices,
    .settings = yosemite_settings,
    .init_func = &initialize_yosemite
};

REGISTER_MACHINE(pmg3nw, pmg3nw_descriptor);
