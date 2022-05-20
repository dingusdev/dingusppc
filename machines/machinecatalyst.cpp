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

/** @file Constructs a Catalyst (Power Macintosh 7200) machine. */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/pci/bandit.h>
#include <devices/common/scsi/scsi.h>
#include <devices/ioctrl/macio.h>
#include <devices/memctrl/platinum.h>
#include <devices/video/atimach64gx.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>

#include <string>

int create_catalyst(std::string& id)
{
    PlatinumCtrl* platinum_obj;

    if (gMachineObj) {
        LOG_F(ERROR, "Global machine object not empty!");
        return -1;
    }

    LOG_F(INFO, "Initializing the Catalyst hardware...");

    // initialize the global machine object
    gMachineObj.reset(new MachineBase("Catalyst"));

    // add memory controller
    gMachineObj->add_component("Platinum", new PlatinumCtrl());

    // add the ARBus-to-PCI bridge
    gMachineObj->add_component("Bandit1", new Bandit(1, "Bandit-PCI1"));

    PCIHost *pci_host = dynamic_cast<PCIHost*>(gMachineObj->get_comp_by_name("Bandit1"));

    // start the sound server
    gMachineObj->add_component("SoundServer", new SoundServer());

    // add the GrandCentral I/O controller
    gMachineObj->add_component("GrandCentral", new GrandCentral());
    pci_host->pci_register_device(
        32, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("GrandCentral")));

    // HACK: attach temporary ATI Mach64 video card
    //gMachineObj->add_component("AtiMach64", new AtiMach64Gx);
    //pci_host->pci_register_device(
    //    4, dynamic_cast<PCIDevice*>(gMachineObj->get_comp_by_name("AtiMach64")));

    // get (raw) pointer to the memory controller
    platinum_obj = dynamic_cast<PlatinumCtrl*>(gMachineObj->get_comp_by_name("Platinum"));

    // allocate ROM region
    if (!platinum_obj->add_rom_region(0xFFC00000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!\n");
        return -1;
    }

    // plug 8MB RAM DIMM into slot #0
    platinum_obj->insert_ram_dimm(0, Platinum::DRAM_CAP_8MB);

    // allocate and map physical RAM
    platinum_obj->map_phys_ram();

    // add single SCSI bus
    gMachineObj->add_component("SCSI0", new ScsiBus);

    // init virtual CPU and request MPC601
    ppc_cpu_init(platinum_obj, PPC_VER::MPC601, 7833600ULL);

    // post-initialize all devices
    if (gMachineObj->postinit_devices()) {
        LOG_F(ERROR, "Could not post-initialize devices!\n");
        return -1;
    }

    LOG_F(INFO, "Initialization complete.\n");

    return 0;
}
