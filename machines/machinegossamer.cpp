/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-20 divingkatae and maximum
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

#include "cpu/ppc/ppcemu.h"
#include "devices/atirage.h"
#include "devices/machineid.h"
#include "devices/soundserver.h"
#include "devices/macio.h"
#include "devices/mpc106.h"
#include "devices/spdram.h"
#include "devices/viacuda.h"
#include "machinebase.h"
#include "machineproperties.h"
#include <thirdparty/loguru/loguru.hpp>
#include <string>


static void setup_ram_slot(std::string name, int i2c_addr, int capacity_megs) {
    if (!capacity_megs)
        return;

    gMachineObj->add_component(name, new SpdSdram168(i2c_addr));
    SpdSdram168* ram_dimm = dynamic_cast<SpdSdram168*>(gMachineObj->get_comp_by_name(name));
    ram_dimm->set_capacity(capacity_megs);

    /* register RAM DIMM with the I2C bus */
    I2CBus* i2c_bus = dynamic_cast<I2CBus*>(gMachineObj->get_comp_by_type(HWCompType::I2C_HOST));
    i2c_bus->register_device(i2c_addr, ram_dimm);
}


int create_gossamer(std::string& id) {
    if (gMachineObj) {
        LOG_F(ERROR, "Global machine object not empty!");
        return -1;
    }

    LOG_F(INFO, "Initializing the Gossamer hardware...");

    /* initialize the global machine object */
    gMachineObj.reset(new MachineBase("Gossamer"));

    /* register MPC106 aka Grackle as memory controller and PCI host */
    gMachineObj->add_component("Grackle", new MPC106);
    gMachineObj->add_alias("Grackle", "PCI_Host");

    /* get raw pointer to MPC106 object */
    MPC106* grackle_obj = dynamic_cast<MPC106*>(gMachineObj->get_comp_by_name("Grackle"));

    /* add the machine ID register */
    gMachineObj->add_component("MachineID", new GossamerID(0x3d8c));
    grackle_obj->add_mmio_region(
        0xFF000004, 4096, dynamic_cast<MMIODevice*>(gMachineObj->get_comp_by_name("MachineID")));

    gMachineObj->add_component("SoundServer", new SoundServer());

    /* add the Heathrow I/O controller */
    gMachineObj->add_component("Heathrow", new HeathrowIC);
    grackle_obj->pci_register_device(
        16, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("Heathrow")));

    /* allocate ROM region */
    if (!grackle_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!\n");
        return -1;
    }

    /* configure RAM slots */
    setup_ram_slot("RAM_DIMM_1", 0x57,  GET_INT_PROP("rambank1_size"));
    setup_ram_slot("RAM_DIMM_2", 0x56,  GET_INT_PROP("rambank2_size"));
    setup_ram_slot("RAM_DIMM_3", 0x55,  GET_INT_PROP("rambank3_size"));

    /* register ATI 3D Rage Pro video card with the PCI host bridge */
    gMachineObj->add_component("ATIRage",
        new ATIRage(ATI_RAGE_PRO_DEV_ID, GET_INT_PROP("gfxmem_size")));
    grackle_obj->pci_register_device(
        18, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("ATIRage")));

    /* Init virtual CPU and request MPC750 CPU aka G3 */
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750);

    LOG_F(INFO, "Initialization complete.\n");

    return 0;
}
