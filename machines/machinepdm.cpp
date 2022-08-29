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

/** @file Construct a PDM-style PowerMacintosh machine.

    Author: Max Poliakovski
 */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/machineid.h>
#include <devices/common/scsi/scsi.h>
#include <devices/memctrl/hmc.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <string>
#include <vector>

void setup_pds()
{
    std::string dev_name = GET_STR_PROP("pds");
    if (!dev_name.empty()) {
        if (!DeviceRegistry::device_registered(dev_name)) {
            LOG_F(WARNING, "specified PDS device %s doesn't exist", dev_name.c_str());
            goto empty_slot;
        }

        // attempt to create device object
        auto dev_obj = DeviceRegistry::get_descriptor(dev_name).m_create_func();

        // safety check
        if (!dev_obj->supports_type(HWCompType::PDS_DEV)) {
            LOG_F(WARNING, "Cannot use device %s with the PDS", dev_name.c_str());
            goto empty_slot;
        }

        // add device to the machine object
        gMachineObj->add_device(dev_name, std::move(dev_obj));

        LOG_F(INFO, "PDS slot: %s", dev_name.c_str());
        return;
    }

empty_slot:
    LOG_F(INFO, "PDS slot: empty");
}

int initialize_pdm(std::string& id)
{
    uint16_t machine_id;

    // get raw pointer to HMC object
    HMC* hmc_obj = dynamic_cast<HMC*>(gMachineObj->get_comp_by_name("HMC"));

    if (id == "pm6100") {
        machine_id = 0x3010;
    } else if (id == "pm7100") {
        machine_id = 0x3012;
    } else if (id == "pm8100") {
        machine_id = 0x3013;
    } else {
        LOG_F(ERROR, "Unknown machine ID: %s!", id.c_str());
        return -1;
    }

    // create machine ID register
    //gMachineObj->add_component("MachineID", new NubusMacID(machine_id));
    gMachineObj->add_device("MachineID", std::unique_ptr<NubusMacID>(new NubusMacID(machine_id)));
    hmc_obj->add_mmio_region(0x5FFFFFFC, 4,
        dynamic_cast<MMIODevice*>(gMachineObj->get_comp_by_name("MachineID")));

    // allocate ROM region
    if (!hmc_obj->add_rom_region(0x40000000, 0x400000)) {
        LOG_F(ERROR, "Could not allocate ROM region!");
        return -1;
    }

    // mirror ROM to 0xFFC00000 for a PowerPC CPU to start
    if (!hmc_obj->add_mem_mirror(0xFFC00000, 0x40000000)) {
        LOG_F(ERROR, "Could not create ROM mirror!");
        return -1;
    }

    // add 8MB of soldered on-board RAM
    if (!hmc_obj->add_ram_region(0x00000000, 0x800000)) {
        LOG_F(ERROR, "Could not allocate built-in RAM region!");
        return -1;
    }

    // add internal SCSI bus
    gMachineObj->add_device("SCSI0", std::unique_ptr<ScsiBus>(new ScsiBus()));

    // set up the processor direct slot
    setup_pds();

    // Init virtual CPU and request MPC601
    ppc_cpu_init(hmc_obj, PPC_VER::MPC601, 7812500ULL);

    return 0;
}

// Monitors supported by the PDM on-board video.
// see displayid.cpp for the full list of supported monitor IDs.
static const vector<string> PDMBuiltinMonitorIDs = {
    "PortraitGS", "MacRGB12in", "MacRGB15in", "HiRes12-14in", "VGA-SVGA",
    "MacRGB16in", "Multiscan15in", "Multiscan17in", "Multiscan20in",
    "NotConnected"
};

static const PropMap pm6100_settings = {
    {"rambank1_size",
        new IntProperty(0, vector<uint32_t>({0, 8, 16, 32, 64, 128}))},
    {"rambank2_size",
        new IntProperty(0, vector<uint32_t>({0, 8, 16, 32, 64, 128}))},
    {"mon_id",
        new StrProperty("HiRes12-14in", PDMBuiltinMonitorIDs)},
    {"pds",
        new StrProperty("")},
    {"emmo",
        new BinProperty(0)},
};

static vector<string> pm6100_devices = {
    "HMC", "Amic"
};

static const MachineDescription pm6100_descriptor = {
    .name = "pm6100",
    .description = "Power Macintosh 6100",
    .devices = pm6100_devices,
    .settings = pm6100_settings,
    .init_func = initialize_pdm
};

// self-registration with the MachineFactory
REGISTER_MACHINE(pm6100, pm6100_descriptor);
