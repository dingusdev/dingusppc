/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
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

/** @file Construct a PDM-style PowerMacintosh machine.

    Author: Max Poliakovski
 */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/machineid.h>
#include <devices/common/scsi/scsi.h>
#include <devices/floppy/superdrive.h>
#include <devices/ioctrl/amic.h>
#include <devices/memctrl/hmc.h>
#include <devices/sound/soundserver.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machineproperties.h>

#include <string>

int create_pdm(std::string& id) {
    if (gMachineObj) {
        LOG_F(ERROR, "PDM Factory: global machine object not empty!");
        return -1;
    }

    LOG_F(INFO, "Initializing the %s hardware...", id.c_str());

    /* initialize the global machine object */
    gMachineObj.reset(new MachineBase("PDM"));

    /* register HMC memory controller */
    gMachineObj->add_component("HMC", new HMC);

    /* start the sound server. */
    gMachineObj->add_component("SoundServer", new SoundServer());

    /* register AMIC I/O controller */
    gMachineObj->add_component("AMIC", new AMIC);

    /* get raw pointer to HMC object */
    HMC* hmc_obj = dynamic_cast<HMC*>(gMachineObj->get_comp_by_name("HMC"));

    // allocate machine ID register and tell we're running PowerMac 6100
    // TODO: add a possibility to select another machine
    // to be used with the same ROM
    gMachineObj->add_component("MachineID", new NubusMacID(0x3010));
    hmc_obj->add_mmio_region(0x5FFFFFFC, 4,
        dynamic_cast<MMIODevice*>(gMachineObj->get_comp_by_name("MachineID")));

    /* allocate ROM region */
    if (!hmc_obj->add_rom_region(0x40000000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!\n");
        return -1;
    }

    /* mirror ROM to 0xFFC00000 for a PowerPC CPU to start */
    if (!hmc_obj->add_mem_mirror(0xFFC00000, 0x40000000)) {
        LOG_F(ERROR, "Could not create ROM mirror!\n");
        return -1;
    }

    /* add 8MB of soldered on-board RAM */
    if (!hmc_obj->add_ram_region(0x00000000, 0x800000)) {
        LOG_F(ERROR, "Could not allocate built-in RAM region!\n");
        return -1;
    }

    /* add internal SCSI bus */
    gMachineObj->add_component("SCSI0", new ScsiBus);

    /* Init virtual CPU and request MPC601 */
    ppc_cpu_init(hmc_obj, PPC_VER::MPC601);

    // post-initialize all devices
    if (gMachineObj->postinit_devices()) {
        LOG_F(ERROR, "Could not post-initialize devices!\n");
        return -1;
    }

    // if a floppy image was given "insert" it into the virtual superdrive
    std::string fd_image_path = GET_STR_PROP("fdd_img");
    std::string fd_write_prot = GET_STR_PROP("fdd_wr_prot");
    if (!fd_image_path.empty()) {
        using namespace MacSuperdrive;

        MacSuperDrive* fdd = dynamic_cast<MacSuperDrive*>
            (gMachineObj->get_comp_by_name("Superdrive"));

        bool write_flag = false;

        if (!fd_write_prot.empty()) {
            if ((fd_write_prot.compare("ON") == 0) or (fd_write_prot.compare("on") == 0)) {
                write_flag = true;
            }
        }

        fdd->insert_disk(fd_image_path, write_flag);
    }

    LOG_F(INFO, "Initialization completed.\n");

    return 0;
}
