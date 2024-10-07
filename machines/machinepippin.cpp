/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

/** @file Bandai/Atmark Pippin emulation. */

/**
    Kudos to Keith Kaisershot @ blitter.net for his precious technical help!
*/

#include <cpu/ppc/ppcemu.h>
#include <devices/common/pci/pcihost.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/aspen.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <loguru.hpp>

int initialize_pippin(std::string& id) {
    LOG_F(INFO, "Building machine Pippin...");

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("AspenPci1"));

    // connect GrandCentral I/O controller to the PCI1 bus
    pci_host->pci_register_device(DEV_FUN(0x10,0),
        dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentral")));

    // get (raw) pointer to the memory controller
    AspenCtrl* aspen_obj = dynamic_cast<AspenCtrl*>(gMachineObj->get_comp_by_name("Aspen"));

    // allocate ROM region
    if (!aspen_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM
    aspen_obj->insert_ram_dimm(0, GET_INT_PROP("rambank1_size")); // soldered onboard RAM
    aspen_obj->insert_ram_dimm(1, GET_INT_PROP("rambank2_size")); // soldered onboard RAM
    aspen_obj->insert_ram_dimm(2, GET_INT_PROP("rambank3_size")); // RAM expansion slot
    aspen_obj->insert_ram_dimm(3, GET_INT_PROP("rambank4_size")); // RAM expansion slot

    // init virtual CPU
    ppc_cpu_init(aspen_obj, PPC_VER::MPC603, false, 16500000ULL);

    return 0;
}

static const PropMap Pippin_Settings = {
    {"rambank1_size",
        new IntProperty(4, vector<uint32_t>({4}))}, // fixed size
    {"rambank2_size",
        new IntProperty(1, vector<uint32_t>({1}))}, // fixed size
    {"rambank3_size",
        new IntProperty(0, vector<uint32_t>({0, 1, 4, 8, 16}))},
    {"rambank4_size",
        new IntProperty(0, vector<uint32_t>({0, 1, 4, 8, 16}))},
    {"emmo",
        new BinProperty(0)},
    {"adb_devices",
        new StrProperty("AppleJack,Keyboard")},
};

static vector<string> Pippin_Devices = {
    "Aspen", "AspenPci1", "ScsiMesh", "MeshTnt", "GrandCentral", "TaosVideo"
};

static const MachineDescription Pippin_Descriptor = {
    .name = "pippin",
    .description = "Bandai Pippin",
    .devices = Pippin_Devices,
    .settings = Pippin_Settings,
    .init_func = &initialize_pippin
};

REGISTER_MACHINE(pippin, Pippin_Descriptor);
