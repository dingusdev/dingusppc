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

/** @file Constructs a Catalyst (Power Macintosh 7200) machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/memctrl/platinum.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <memctrl/memctrlbase.h>

#include <string>

int initialize_catalyst(std::string& id)
{
    LOG_F(INFO, "Building machine Catalyst...");

    PlatinumCtrl* platinum_obj;

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("Bandit1"));

    // add the GrandCentral I/O controller
    pci_host->pci_register_device(
        DEV_FUN(0x10,0), dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("GrandCentral")));

    // get (raw) pointer to the memory controller
    platinum_obj = dynamic_cast<PlatinumCtrl*>(gMachineObj->get_comp_by_name("Platinum"));

    // allocate ROM region
    if (!platinum_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // plug 8MB RAM DIMM into slot #0
    platinum_obj->insert_ram_dimm(0, DRAM_CAP_8MB);

    // allocate and map physical RAM
    platinum_obj->map_phys_ram();

    // init virtual CPU and request MPC601
    ppc_cpu_init(platinum_obj, PPC_VER::MPC601, 7833600ULL);

    return 0;
}

static const PropMap pm7200_settings = {
    {"rambank1_size",
        new IntProperty(16, vector<uint32_t>({4, 8, 16, 32, 64, 128}))},
    {"rambank2_size",
        new IntProperty( 0, vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"rambank3_size",
        new IntProperty( 0, vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"rambank4_size",
        new IntProperty( 0, vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"emmo",
        new BinProperty(0)},
};

static vector<string> pm7200_devices = {
    "Platinum", "Bandit1", "GrandCentral"
};

static const MachineDescription pm7200_descriptor = {
    .name = "pm7200",
    .description = "Power Macintosh 7200",
    .devices = pm7200_devices,
    .settings = pm7200_settings,
    .init_func = &initialize_catalyst
};

REGISTER_MACHINE(pm7200, pm7200_descriptor);
