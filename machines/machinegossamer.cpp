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

#include <thirdparty/loguru.hpp>
#include "machinebase.h"
#include "devices/mpc106.h"
#include "devices/machineid.h"
#include "devices/macio.h"
#include "cpu/ppc/ppcemu.h"
#include "machinebase.h"

int create_gossamer()
{
    if (gMachineObj) {
        LOG_F(ERROR, "Global machine object not empty!");
        return -1;
    }

    LOG_F(INFO, "Initializing the Gossamer hardware...");

    /* initialize the global machine object */
    gMachineObj.reset(new MachineBase("Gossamer"));

    /* register MPC106 aka Grackle as memory controller and PCI host */
    gMachineObj->add_component("Grackle", HWCompType::MEM_CTRL, new MPC106);
    gMachineObj->add_alias("Grackle", "PCI_Host", HWCompType::PCI_HOST);

    /* get raw pointer to MPC106 object */
    MPC106 *grackle_obj = dynamic_cast<MPC106 *>(gMachineObj->get_comp_by_name("Grackle"));

    /* add the machine ID register */
    gMachineObj->add_component("MachineID", HWCompType::MMIO_DEV,
        new GossamerID(0x3d8c));
    grackle_obj->add_mmio_region(0xFF000004, 4096,
        gMachineObj->get_comp_by_name("MachineID"));

    /* add the Heathrow I/O controller */
    gMachineObj->add_component("Heathrow", HWCompType::MMIO_DEV, new HeathrowIC);
    grackle_obj->pci_register_device(16,
        dynamic_cast<PCIDevice *>(gMachineObj->get_comp_by_name("Heathrow")));

    /* allocate ROM region */
    if (!grackle_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!\n");
        return -1;
    }

    /* Init virtual CPU and request MPC750 CPU aka G3 */
    ppc_cpu_init(grackle_obj, PPC_VER::MPC750);

    LOG_F(INFO, "Initialization complete.\n");

    return 0;
}
