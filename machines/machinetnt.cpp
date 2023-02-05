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

/** @file Constructs a TNT (Power Macintosh 7500, 8500 etc) machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/pci/bandit.h>
#include <devices/common/scsi/scsi.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/hammerhead.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <memory>
#include <string>

int initialize_tnt(std::string& id)
{
    LOG_F(INFO, "Building machine TNT...");

    HammerheadCtrl* memctrl_obj;

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("Bandit1"));

    // connect GrandCentral I/O controller to the PCI1 bus
    pci_host->pci_register_device(
        DEV_FUN(0x10,0), dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("GrandCentral")));

    // get video PCI controller object
    PCIHost *vci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("Chaos"));

    // connect built-in video device to the VCI bus
    vci_host->pci_register_device(
        DEV_FUN(0x0B,0), dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("ControlVideo")));

    // get (raw) pointer to the memory controller
    memctrl_obj = dynamic_cast<HammerheadCtrl*>(gMachineObj->get_comp_by_name("Hammerhead"));

    // allocate ROM region
    if (!memctrl_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // plug 16MB RAM DIMM into slot #0
    memctrl_obj->insert_ram_dimm(0, DRAM_CAP_16MB);

    // allocate and map physical RAM
    memctrl_obj->map_phys_ram();

    // add single SCSI bus
    gMachineObj->add_device("SCSI0", std::unique_ptr<ScsiBus>(new ScsiBus()));

    // init virtual CPU and request MPC601
    ppc_cpu_init(memctrl_obj, PPC_VER::MPC601, 7833600ULL);

    return 0;
}

static const PropMap pm7500_settings = {
    {"rambank1_size",
        new IntProperty(16, vector<uint32_t>({4, 8, 16, 32, 64, 128}))},
    {"rambank2_size",
        new IntProperty( 0, vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"rambank3_size",
        new IntProperty( 0, vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"rambank4_size",
        new IntProperty( 0, vector<uint32_t>({0, 4, 8, 16, 32, 64, 128}))},
    {"gfxmem_size",
        new IntProperty( 1, vector<uint32_t>({1, 2, 4}))},
    {"emmo",
        new BinProperty(0)},
};

static vector<string> pm7500_devices = {
    "Hammerhead", "Bandit1", "Chaos", "GrandCentral", "ControlVideo"
};

static const MachineDescription pm7500_descriptor = {
    .name = "pm7500",
    .description = "Power Macintosh 7500",
    .devices = pm7500_devices,
    .settings = pm7500_settings,
    .init_func = &initialize_tnt
};

REGISTER_MACHINE(pm7500, pm7500_descriptor);
