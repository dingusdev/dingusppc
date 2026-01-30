/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 divingkatae and maximum
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
#include <devices/deviceregistry.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <machines/machine.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

static const std::vector<PciIrqMap> grackle_irq_map = {
    {nullptr    , DEV_FUN(0x00,0),                  }, // Grackle
    {"pci_A1"   , DEV_FUN(0x0D,0), IntSrc::PCI_A    },
    {"pci_B1"   , DEV_FUN(0x0E,0), IntSrc::PCI_B    },
    {"pci_C1"   , DEV_FUN(0x0F,0), IntSrc::PCI_C    },
    {nullptr    , DEV_FUN(0x10,0),                  }, // Paddington I/O controller
    {"pci_GPU"  , DEV_FUN(0x12,0), IntSrc::PCI_GPU  },
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

class MachineBondi : public Machine {
public:
    int initialize(const std::string &id);
};

int MachineBondi::initialize(const std::string &id) {
    LOG_F(INFO, "Building machine Bondi...");

    // get pointer to the memory controller/primary PCI bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("Grackle"));
    grackle_obj->set_irq_map(grackle_irq_map);

    MacIoTwo* mio_obj = dynamic_cast<MacIoTwo*>(gMachineObj->get_comp_by_name("Paddington"));
    mio_obj->set_media_bay_id(0x30);

    grackle_obj->pci_register_device(DEV_FUN(0x10,0), mio_obj);

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFF00000, 0x100000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM slots
    // First ram slot is enumerated twice for some reason, the second slot is never
    // enumerated, so make sure both slots have the same RAM.
    uint32_t bank_1_size = GET_INT_PROP("rambank1_size");
    uint32_t bank_2_size = GET_INT_PROP("rambank2_size");
    if (bank_1_size != bank_2_size)
        LOG_F(ERROR, "rambank1_size and rambank2_size should have equal size");
    setup_ram_slot("RAM_DIMM_1", 0x50, bank_1_size);
    setup_ram_slot("RAM_DIMM_2", 0x51, bank_2_size);

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
    {"rambank1_size", new IntProperty(128, std::vector<uint32_t>({8, 16, 32, 64, 128, 256, 512}))},
    {"rambank2_size", new IntProperty(128, std::vector<uint32_t>({8, 16, 32, 64, 128, 256, 512}))},
    {"emmo", new BinProperty(0)},
    {"hdd_config", new StrProperty("Ide0:0")},
    {"cdr_config", new StrProperty("Ide1:0")},
    {"pci_GPU", new StrProperty("AtiRagePro")},
};

static std::vector<std::string> bondi_devices = {
    "Grackle", "BurgundySnd", "Paddington", "AtaHardDisk", "AtapiCdrom"};

static const DeviceDescription MachineBondi_descriptor = {
    Machine::create<MachineBondi>, bondi_devices, bondi_settings
};

REGISTER_DEVICE(MachineBondi, MachineBondi_descriptor);

static const MachineDescription bondi_descriptor = {
    .name        = "imacg3",
    .description = "iMac G3 Bondi Blue",
    .machine_root = "MachineBondi",
};

REGISTER_MACHINE(imacg3, bondi_descriptor);
