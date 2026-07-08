/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

/** @file Alchemy machines (Performa 6400, Power Macintosh 5400) . */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/pci/pcihost.h>
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/psx.h>
#include <loguru.hpp>
#include <machines/machine.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <string>

class MachineAlchemy : public Machine {
public:
    static std::unique_ptr<HWComponent> create5400() {
        return Machine::create_with_id<MachineAlchemy>("pm5400");
    }

    static std::unique_ptr<HWComponent> create6400() {
        return Machine::create_with_id<MachineAlchemy>("pm6400");
    }

    int initialize(const std::string &id);
};

int MachineAlchemy::initialize(const std::string &id) {
    LOG_F(INFO, "Building machine Alchemy...");

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("PsxPci1"));
    //pci_host->set_irq_map(psx_irq_map);

    MacIoTwo* macio_obj = dynamic_cast<MacIoTwo*>(gMachineObj->get_comp_by_name("OHare"));

    // set Box aka CPU ID and activate factory tests if requested.
    // Please note that Performa 6400 ROM 6F5724C0 contains a bug
    // that prevents activation of the factory tests.
    // To fix that, the said ROM needs to be patched as follows:
    // - change 0x7C631A79 (xor. r3, r3, r3) at 0xFFF20408 to
    //   0x7C631A78 (xor r3, r3, r3)
    macio_obj->set_cpu_id(
        (
            (id == "pm6400") ?
                0xE0 // for Performa 6400 => CPUID0 (BOXID0) pulled low
            :
                0xF0 // for PowerMac 5400 => CPUID0...CPUID3 pulled high
        ) & (
            GET_BIN_PROP("emmo") ?
                ~0x40 // pull BOXID2 low to activate factory tests
            :
                ~0    // No-op
        )
    );

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
    psx_obj->insert_ram_dimm(0, GET_INT_PROP("rambank1_size") * DRAM_CAP_1MB);
    psx_obj->insert_ram_dimm(1, GET_INT_PROP("rambank2_size") * DRAM_CAP_1MB);
    psx_obj->insert_ram_dimm(2, GET_INT_PROP("rambank3_size") * DRAM_CAP_1MB);
    psx_obj->insert_ram_dimm(3, GET_INT_PROP("rambank4_size") * DRAM_CAP_1MB);
    psx_obj->insert_ram_dimm(4, GET_INT_PROP("rambank5_size") * DRAM_CAP_1MB);

    // allocate and map physical RAM
    psx_obj->map_phys_ram();

    // configure CPU clocks
    uint64_t bus_freq      = 40000000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    psx_obj->set_bus_speed(PSX_BUS_SPEED_40);

    // init virtual CPU and request MPC603ev
    ppc_cpu_init(psx_obj, PPC_VER::MPC603E, false, timebase_freq);

    // CPU frequency is hardcoded to 225 MHz for now
    //ppc_state.spr[SPR::HID1] = get_cpu_pll_value(225000000) << 28;

    return 0;
}

static const PropMap pm6400_settings = {
    {"rambank1_size",
        new IntProperty(32, std::vector<uint32_t>({   4, 8, 16, 32}))},
    {"rambank2_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32}))},
    {"rambank3_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32}))},
    {"rambank4_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32}))},
    {"rambank5_size",
        new IntProperty( 0, std::vector<uint32_t>({0, 4, 8, 16, 32}))},
    {"hdd_config",
        new StrProperty("Ide0:0")},
    {"emmo",
        new BinProperty(0)},
};

static std::vector<std::string> pm6400_devices = {
    "Psx", "PsxPci1", "ScreamerSnd", "OHare", "ValkyrieAlchemy", "AtaHardDisk"
};

static const DeviceDescription Machine5400_descriptor = {
    MachineAlchemy::create5400, pm6400_devices, pm6400_settings
};

static const DeviceDescription Machine6400_descriptor = {
    MachineAlchemy::create6400, pm6400_devices, pm6400_settings
};

REGISTER_DEVICE(MachineAlchemy5400, Machine5400_descriptor);
REGISTER_DEVICE(MachineAlchemy6400, Machine6400_descriptor);

static const MachineDescription pm5400_descriptor = {
    .name = "pm5400",
    .description = "Power Macintosh 5400",
    .machine_root = "MachineAlchemy5400"
};

static const MachineDescription pm6400_descriptor = {
    .name = "pm6400",
    .description = "Performa 6400",
    .machine_root = "MachineAlchemy6400"
};

REGISTER_MACHINE(pm5400, pm5400_descriptor);
REGISTER_MACHINE(pm6400, pm6400_descriptor);
