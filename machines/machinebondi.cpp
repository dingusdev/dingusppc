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

/** @file Construct the Bondi machine (iMac G3). */

#include <cpu/ppc/ppcemu.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

static const std::vector<PciIrqMap> grackle_irq_map = {
    {nullptr    , DEV_FUN(0x00,0),                  }, // Grackle
    {"pci_A1"   , DEV_FUN(0x0D,0), IntSrc::PCI_A    },
    {"pci_B1"   , DEV_FUN(0x0E,0), IntSrc::PCI_B    },
    {"pci_C1"   , DEV_FUN(0x0F,0), IntSrc::PCI_C    },
    {nullptr    , DEV_FUN(0x10,0),                  },
    {"pci_GPU"  , DEV_FUN(0x12,0), IntSrc::PCI_GPU  }, // Heathrow
    {"pci_PERCH", DEV_FUN(0x14,0), IntSrc::PCI_PERCH},
};

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

int initialize_bondi(std::string& id) {
    LOG_F(INFO, "Building machine Bondi...");

    // get pointer to the memory controller/primary PCI bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("Grackle"));
    grackle_obj->set_irq_map(grackle_irq_map);

    HeathrowIC* heathrow = dynamic_cast<HeathrowIC*>(gMachineObj->get_comp_by_name("Heathrow"));
    heathrow->set_media_bay_id(0x30);

    grackle_obj->pci_register_device(DEV_FUN(0x10,0), heathrow);

    grackle_obj->pci_register_device(
        DEV_FUN(0x12,0), dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("AtiMach64Gx")));

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFF00000, 0x100000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM slots
    // First ram slot is enumerated twice for some reason, the second slot is never enumerated, so
    // put half the ram in each slot.
    setup_ram_slot("RAM_DIMM_1", 0x50, GET_INT_PROP("rambank1_size") / 2);
    setup_ram_slot("RAM_DIMM_2", 0x51, GET_INT_PROP("rambank1_size") / 2);

    // configure CPU clocks
    uint64_t bus_freq      = 66820000ULL;
    uint64_t timebase_freq = bus_freq / 4;

    // initialize virtual CPU and request MPC750 CPU aka G3
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, false, timebase_freq);

    // set CPU PLL ratio to 3.5
    ppc_state.spr[SPR::HID1] = 0xE << 28;

    return 0;
}

static const PropMap bondi_settings = {
    {"rambank1_size", new IntProperty(128, std::vector<uint32_t>({32, 64, 128, 256, 512, 1024}))},
    {"emmo", new BinProperty(0)},
    {"hdd_config", new StrProperty("Ide0:0")},
    {"cdr_config", new StrProperty("Ide1:0")},
    {"pci_GPU", new StrProperty("AtiRagePro")},
};

static std::vector<std::string> bondi_devices = {
    "Grackle", "BurgundySnd", "Heathrow", "AtiMach64Gx", "AtaHardDisk", "AtapiCdrom"};

static const MachineDescription bondi_descriptor = {
    .name        = "imacg3",
    .description = "iMac G3 Bondi Blue",
    .devices     = bondi_devices,
    .settings    = bondi_settings,
    .init_func   = &initialize_bondi};

REGISTER_MACHINE(imacg3, bondi_descriptor);
