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

/** @file Constructs the Gossamer machine.

    Author: Max Poliakovski
 */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/i2c/athens.h>
#include <devices/common/i2c/i2cprom.h>
#include <devices/common/machineid.h>
#include <devices/floppy/floppyimg.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/mpc106.h>
#include <devices/memctrl/spdram.h>
#include <devices/sound/soundserver.h>
#include <devices/video/atirage.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <memory>
#include <string>

// EEPROM ID content for a Whisper personality card.
const uint8_t WhisperID[16] = {
    0x0F, 0xAA, 0x55, 0xAA, 0x57, 0x68, 0x69, 0x73, 0x70, 0x65, 0x72, 0x00,
    0x00, 0x00, 0x00, 0x02
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


int initialize_gossamer(std::string& id)
{
    // get pointer to the memory controller/PCI host bridge object
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("Grackle"));

    // add the machine ID register
    gMachineObj->add_device("MachineID", std::unique_ptr<GossamerID>(new GossamerID(0xBF3D)));
    grackle_obj->add_mmio_region(
        0xFF000004, 4096, dynamic_cast<MMIODevice*>(gMachineObj->get_comp_by_name("MachineID")));

    // register the Heathrow I/O controller with the PCI host bridge
    grackle_obj->pci_register_device(
        16, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Heathrow")));

    // allocate ROM region
    if (!grackle_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // configure RAM slots
    setup_ram_slot("RAM_DIMM_1", 0x57, GET_INT_PROP("rambank1_size"));
    setup_ram_slot("RAM_DIMM_2", 0x56, GET_INT_PROP("rambank2_size"));
    setup_ram_slot("RAM_DIMM_3", 0x55, GET_INT_PROP("rambank3_size"));

    // select built-in GPU name
    std::string gpu_name = "AtiRageGT";
    if (id == "pmg3twr") {
        gpu_name = "AtiRagePro";
    }

    // register built-in ATI Rage GPU with the PCI host bridge
    grackle_obj->pci_register_device(
        18, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name(gpu_name)));

    // add Athens clock generator device and register it with the I2C host
    gMachineObj->add_device("Athens", std::unique_ptr<AthensClocks>(new AthensClocks(0x28)));
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(0x28, dynamic_cast<I2CDevice*>(gMachineObj->get_comp_by_name("Athens")));

    // create ID EEPROM for the Whisper personality card and register it with the I2C host
    gMachineObj->add_device("Perch", std::unique_ptr<I2CProm>(new I2CProm(0x53, 256)));
    I2CProm* perch_id = dynamic_cast<I2CProm*>(gMachineObj->get_comp_by_name("Perch"));
    perch_id->fill_memory(0, 256, 0);
    perch_id->fill_memory(32, 223, 0xFF);
    perch_id->set_memory(0, WhisperID, sizeof(WhisperID));
    i2c_bus->register_device(0x53, perch_id);

    // initialize virtual CPU and request MPC750 CPU aka G3
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, 16705000ULL);

    return 0;
}

static const PropMap gossamer_settings = {
    {"rambank1_size",
        new IntProperty(256, vector<uint32_t>({8, 16, 32, 64, 128, 256}))},
    {"rambank2_size",
        new IntProperty(  0, vector<uint32_t>({0, 8, 16, 32, 64, 128, 256}))},
    {"rambank3_size",
        new IntProperty(  0, vector<uint32_t>({0, 8, 16, 32, 64, 128, 256}))},
    {"emmo",
        new BinProperty(0)},
};

static vector<string> pmg3_devices = {
    "Grackle", "Heathrow", "AtiRageGT"
};

static const MachineDescription pmg3dt_descriptor = {
    .name = "pmg3dt",
    .description = "Power Macintosh G3 (Beige) Desktop",
    .devices = pmg3_devices,
    .settings = gossamer_settings,
    .init_func = &initialize_gossamer
};

REGISTER_MACHINE(pmg3dt, pmg3dt_descriptor);
