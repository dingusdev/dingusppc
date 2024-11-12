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

/** @file Constructs a TNT (Power Macintosh 7500, 8500 etc) machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/machineid.h>
#include <devices/common/pci/pcihost.h>
#include <devices/common/pci/pcidevice.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/hammerhead.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <string>

static std::vector<PciIrqMap> bandit1_irq_map = {
    {nullptr , DEV_FUN(0x0B,0), IntSrc::BANDIT1},
    {"pci_A1", DEV_FUN(0x0D,0), IntSrc::PCI_A},
    {"pci_B1", DEV_FUN(0x0E,0), IntSrc::PCI_B},
    {"pci_C1", DEV_FUN(0x0F,0), IntSrc::PCI_C},
    {nullptr , DEV_FUN(0x10,0),              }, // GrandCentral
};

static std::vector<PciIrqMap> bandit2_irq_map = {
    {nullptr , DEV_FUN(0x0B,0), IntSrc::BANDIT2},
    {"pci_D2", DEV_FUN(0x0D,0), IntSrc::PCI_D},
    {"pci_E2", DEV_FUN(0x0E,0), IntSrc::PCI_E},
    {"pci_F2", DEV_FUN(0x0F,0), IntSrc::PCI_F},
};

static std::vector<PciIrqMap> chaos_irq_map = {
    {nullptr,  DEV_FUN(0x0B,0), IntSrc::CONTROL},
    {"vci_D",  DEV_FUN(0x0D,0), IntSrc::PLANB},
    {"vci_E",  DEV_FUN(0x0E,0), IntSrc::VCI  },
};

int initialize_tnt(std::string& id)
{
    LOG_F(INFO, "Building machine TNT...");

    HammerheadCtrl* memctrl_obj;

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("Bandit1"));
    pci_host->set_irq_map(bandit1_irq_map);

    // get (raw) pointer to the I/O controller
    GrandCentral* gc_obj = dynamic_cast<GrandCentral*>(gMachineObj->get_comp_by_name("GrandCentral"));

    // connect GrandCentral I/O controller to the PCI1 bus
    pci_host->pci_register_device(
        DEV_FUN(0x10,0), gc_obj);

    // get video PCI controller object
    PCIHost *vci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("Chaos"));
    if (vci_host) {
        vci_host->set_irq_map(chaos_irq_map);
        // connect built-in video device to the VCI bus
        vci_host->pci_register_device(
            DEV_FUN(0x0B,0), dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("ControlVideo")));
    }

    // attach IOBus Device #1 0xF301A000
    gMachineObj->add_device("BoardReg1", std::unique_ptr<BoardRegister>(
        new BoardRegister("Board Register 1",
            0x3F                                                                       | // pull up all PRSNT bits
            ((GET_BIN_PROP("emmo") ^ 1) << 8)                                          | // factory tests (active low)
            ((gMachineObj->get_comp_by_name_optional("Sixty6Video") == nullptr) << 13) | // composite video out (active low)
            ((gMachineObj->get_comp_by_name_optional("MeshTnt") != nullptr) << 14)     | // fast SCSI (active high)
            0x8000U                                                                      // pull up unused bits
    )));
    gc_obj->attach_iodevice(0, dynamic_cast<BoardRegister*>(gMachineObj->get_comp_by_name("BoardReg1")));

    PCIHost *pci2_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name_optional("Bandit2"));
    if (pci2_host) {
        pci2_host->set_irq_map(bandit2_irq_map);
        // attach IOBus Device #3 0xF301C000
        gMachineObj->add_device("BoardReg2", std::unique_ptr<BoardRegister>(
            new BoardRegister("Board Register 2",
                0x3F                                        | // pull up all PRSNT bits
                0x8000U                                       // pull up unused bits
        )));
        gc_obj->attach_iodevice(2, dynamic_cast<BoardRegister*>(gMachineObj->get_comp_by_name("BoardReg2")));
    }

    // get (raw) pointer to the memory controller
    memctrl_obj = dynamic_cast<HammerheadCtrl*>(gMachineObj->get_comp_by_name("Hammerhead"));

    memctrl_obj->set_motherboard_id((vci_host ? Hammerhead::MBID_VCI0_PRESENT : 0) |
                                    (pci2_host ? Hammerhead::MBID_PCI2_PRESENT : 0));
    memctrl_obj->set_bus_speed(Hammerhead::BUS_SPEED_50_MHZ);

    // allocate ROM region
    if (!memctrl_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // populate RAM banks from configuration properties
    for (int bank_num = 0; bank_num <= 3; bank_num++) {
        std::string bn = {char('1' + bank_num)};
        int bank_size = GET_INT_PROP("rambank" + bn + "_size");
        if (bank_size)
            memctrl_obj->insert_ram_dimm(bank_num, bank_size * DRAM_CAP_1MB);
    }

    // allocate and map physical RAM
    memctrl_obj->map_phys_ram();

    // init virtual CPU
    std::string cpu = GET_STR_PROP("cpu");
    if (cpu == "604e")
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC604E, false, 12500000ULL);
    else if (cpu == "604")
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC604, false, 12500000ULL);
    else if (cpu == "601")
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC601, true, 7833600ULL);
    else if (cpu == "750") {
        // configure CPU clocks
        uint64_t bus_freq      = 50000000ULL;
        uint64_t timebase_freq = bus_freq / 4;

        // initialize virtual CPU and request MPC750 CPU aka G3
        ppc_cpu_init(memctrl_obj, PPC_VER::MPC750, false, timebase_freq);

        // set CPU PLL ratio to 3.5
        ppc_state.spr[SPR::HID1] = 0xE << 28;
    }

    return 0;
}

template <uint32_t cpu>
static const PropMap pm7500_settings = {
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
    {"cpu",
        new StrProperty(
            cpu == PPC_VER::MPC601  ? "601" :
            cpu == PPC_VER::MPC604  ? "604" :
            cpu == PPC_VER::MPC604E ? "604e" :
            "604e", vector<std::string>({"601", "604", "604e", "750"})
        )
    },
};

static vector<string> pm7500_devices = {
    "Hammerhead", "Bandit1", "Chaos", "ScsiMesh", "MeshTnt", "GrandCentral",
    "ControlVideo"
};

static vector<string> pm8500_devices = {
    "Hammerhead", "Bandit1", "Chaos", "ScsiMesh", "MeshTnt", "GrandCentral",
    "ControlVideo", "Sixty6Video"
};

static vector<string> pm9500_devices = {
    "Hammerhead", "Bandit1", "Bandit2", "ScsiMesh", "MeshTnt", "GrandCentral"
};

static const MachineDescription pm7300_descriptor = {
    .name = "pm7300",
    .description = "Power Macintosh 7300",
    .devices = pm7500_devices,
    .settings = pm7500_settings<PPC_VER::MPC604E>,
    .init_func = &initialize_tnt
};

static const MachineDescription pm7500_descriptor = {
    .name = "pm7500",
    .description = "Power Macintosh 7500",
    .devices = pm7500_devices,
    .settings = pm7500_settings<PPC_VER::MPC601>,
    .init_func = &initialize_tnt
};

static const MachineDescription pm8500_descriptor = {
    .name = "pm8500",
    .description = "Power Macintosh 8500",
    .devices = pm8500_devices,
    .settings = pm7500_settings<PPC_VER::MPC604>,
    .init_func = &initialize_tnt
};

static const MachineDescription pm9500_descriptor = {
    .name = "pm9500",
    .description = "Power Macintosh 9500",
    .devices = pm9500_devices,
    .settings = pm7500_settings<PPC_VER::MPC604>,
    .init_func = &initialize_tnt
};

static const MachineDescription pm7600_descriptor = {
    .name = "pm7600",
    .description = "Power Macintosh 7600",
    .devices = pm7500_devices,
    .settings = pm7500_settings<PPC_VER::MPC604E>,
    .init_func = &initialize_tnt
};

static const MachineDescription pm8600_descriptor = {
    .name = "pm8600",
    .description = "Power Macintosh 8600",
    .devices = pm8500_devices,
    .settings = pm7500_settings<PPC_VER::MPC604E>,
    .init_func = &initialize_tnt
};

static const MachineDescription pm9600_descriptor = {
    .name = "pm9600",
    .description = "Power Macintosh 9600",
    .devices = pm9500_devices,
    .settings = pm7500_settings<PPC_VER::MPC604E>,
    .init_func = &initialize_tnt
};

REGISTER_MACHINE(pm7300, pm7300_descriptor);
REGISTER_MACHINE(pm7500, pm7500_descriptor);
REGISTER_MACHINE(pm8500, pm8500_descriptor);
REGISTER_MACHINE(pm9500, pm9500_descriptor);
REGISTER_MACHINE(pm7600, pm7600_descriptor);
REGISTER_MACHINE(pm8600, pm8600_descriptor);
REGISTER_MACHINE(pm9600, pm9600_descriptor);
