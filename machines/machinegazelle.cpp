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

/** Constructs a Gazelle style machine. This family includes
    Power Macintosh 5500, 6500 and Twentieth Anniversary Mac.
 */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/hwcomponent.h>
#include <devices/memctrl/psx.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <memctrl/memctrlbase.h>
#include <loguru.hpp>

// TODO: move it to /cpu/ppc
int get_cpu_pll_value(const uint64_t cpu_freq_hz) {
    switch (cpu_freq_hz / 1000000) {
    case 225:
        return 7;  // 4.5:1 ratio
    case 250:
        return 11; // 5:1 ratio
    case 275:
        return 9;  // 5.5:1 ratio
    default:
        ABORT_F("Unsupported CPU frequency %llu Hz", cpu_freq_hz);
    }
}

int initialize_gazelle(std::string& id)
{
    LOG_F(INFO, "Building machine Gazelle...");

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("PsxPci1"));

    // register O'Hare I/O controller with the main PCI bus
    pci_host->pci_register_device(
        DEV_FUN(0x10,0), dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("OHare")));

    PsxCtrl* psx_obj = dynamic_cast<PsxCtrl*>(gMachineObj->get_comp_by_name("Psx"));

    // allocate ROM region
    if (!psx_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // plug 32MB RAM DIMM into slot #0
    psx_obj->insert_ram_dimm(0, DRAM_CAP_32MB);

    // allocate and map physical RAM
    psx_obj->map_phys_ram();

    // configure CPU clocks
    uint64_t bus_freq      = 50000000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // init virtual CPU and request MPC603ev
    ppc_cpu_init(psx_obj, PPC_VER::MPC603EV, timebase_freq);

    // CPU frequency is hardcoded to 225 MHz for now
    ppc_state.spr[SPR::HID1] = get_cpu_pll_value(225000000) << 28;

    return 0;
}

static const PropMap pm6500_settings = {
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

static vector<string> pm6500_devices = {
    "Psx", "PsxPci1", "OHare"
};

static const MachineDescription pm6500_descriptor = {
    .name = "pm6500",
    .description = "Power Macintosh 6500",
    .devices = pm6500_devices,
    .settings = pm6500_settings,
    .init_func = &initialize_gazelle
};

REGISTER_MACHINE(pm6500, pm6500_descriptor);
