/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-25 divingkatae and maximum
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
#include <devices/deviceregistry.h>
#include <machines/machine.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <memctrl/memctrlbase.h>
#include <loguru.hpp>

static std::vector<PciIrqMap> psx_irq_map = {
    {nullptr , DEV_FUN(0x0B,0), IntSrc::BANDIT1},
    {"pci_A1", DEV_FUN(0x0D,0), IntSrc::PCI_A},
    {"pci_B1", DEV_FUN(0x0E,0), IntSrc::PCI_B},
    {"pci_C1", DEV_FUN(0x0F,0), IntSrc::PCI_C},
    {nullptr , DEV_FUN(0x10,0),              }, // OHare
    {"pci_E1", DEV_FUN(0x11,0), IntSrc::PCI_E},
    {"pci_F1", DEV_FUN(0x12,0), IntSrc::PCI_F},
};

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

class MachineGazelle : public Machine {
public:
    int initialize(const std::string &id);
};

int MachineGazelle::initialize(const std::string &id) {
    LOG_F(INFO, "Building machine Gazelle...");

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("PsxPci1"));
    pci_host->set_irq_map(psx_irq_map);

    // register O'Hare I/O controller with the main PCI bus
    pci_host->pci_register_device(
        DEV_FUN(0x10,0), dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("OHare")));

    PsxCtrl* psx_obj = dynamic_cast<PsxCtrl*>(gMachineObj->get_comp_by_name("Psx"));

    // allocate ROM region
    if (!psx_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // insert RAM DIMMs
    psx_obj->insert_ram_dimm(0, GET_INT_PROP("rambank0_size") * DRAM_CAP_1MB);
    psx_obj->insert_ram_dimm(1, GET_INT_PROP("rambank1_size") * DRAM_CAP_1MB);
    psx_obj->insert_ram_dimm(3, GET_INT_PROP("rambank2_size") * DRAM_CAP_1MB);

    // configure CPU clocks
    uint64_t bus_freq      = 50000000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // init virtual CPU and request MPC603ev
    ppc_cpu_init(psx_obj, PPC_VER::MPC603EV, false, timebase_freq);

    // CPU frequency is hardcoded to 225 MHz for now
    ppc_state.spr[SPR::HID1] = get_cpu_pll_value(225000000) << 28;

    return 0;
}

static const PropMap pm6500_settings = {
    {"rambank0_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32    }))},
    {"rambank1_size",
        new IntProperty(32, std::vector<uint32_t>({   4, 8, 16, 32, 64}))},
    {"rambank2_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32, 64}))},
    {"emmo",
        new BinProperty(0)},
    {"hdd_config",
        new StrProperty("Ide0:0")},
    {"pci_F1",
        new StrProperty("AtiRageGT")},
};

static std::vector<std::string> pm6500_devices = {
    "Psx", "PsxPci1", "ScreamerSnd", "OHare", "AtaHardDisk"
};

static const DeviceDescription MachineGazelle_descriptor = {
    Machine::create<MachineGazelle>, pm6500_devices, pm6500_settings
};

REGISTER_DEVICE(MachineGazelle, MachineGazelle_descriptor);

static const MachineDescription pm6500_descriptor = {
    .name = "pm6500",
    .description = "Power Macintosh 6500",
    .machine_root = "MachineGazelle",
};

REGISTER_MACHINE(pm6500, pm6500_descriptor);
