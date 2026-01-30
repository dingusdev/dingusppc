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

/** @file Construct a PDM-style Power Macintosh machine.

    Author: Max Poliakovski
 */

#include <cpu/ppc/ppcemu.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/machineid.h>
#include <devices/common/mmiodevice.h>
#include <devices/common/scsi/scsi.h>
#include <devices/common/scsi/scsicdrom.h>
#include <devices/common/scsi/scsihd.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/hmc.h>
#include <loguru.hpp>
#include <machines/machine.h>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>

#include <string>
#include <vector>

void setup_pds() {
    std::string dev_name = GET_STR_PROP("pds");
    if (!dev_name.empty()) {
        if (!DeviceRegistry::device_registered(dev_name)) {
            LOG_F(WARNING, "specified PDS device %s doesn't exist", dev_name.c_str());
            return;
        }

        // attempt to create device object
        auto dev_obj = DeviceRegistry::get_descriptor(dev_name).m_create_func();

        // safety check
        if (!dev_obj->supports_type(HWCompType::NUBUS_DEV)) {
            LOG_F(WARNING, "Cannot use device %s with the PDS", dev_name.c_str());
            return;
        }

        // add device to the machine object
        gMachineObj->add_device(dev_name, std::move(dev_obj));

        LOG_F(INFO, "Plugged %s into the PDS/VDS slot", dev_name.c_str());
    }
}

class MachinePdm : public Machine {
public:
    static std::unique_ptr<HWComponent> create6100() {
        return Machine::create_with_id<MachinePdm>("pm6100");
    }

    static std::unique_ptr<HWComponent> create7100() {
        return Machine::create_with_id<MachinePdm>("pm7100");
    }

    static std::unique_ptr<HWComponent> create8100() {
        return Machine::create_with_id<MachinePdm>("pm8100");
    }

    int initialize(const std::string &id);
};

int MachinePdm::initialize(const std::string &id) {
    LOG_F(INFO, "Building machine PDM...");

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

    uint32_t bank_a_size = GET_INT_PROP("rambank1_size");
    uint32_t bank_b_size = GET_INT_PROP("rambank2_size");
    if (bank_b_size && bank_a_size != bank_b_size) {
        LOG_F(ERROR, "rambank1_size and rambank2_size should have equal size");
        return -1;
    }

    if (hmc_obj->install_ram(BANK_SIZE_8MB, bank_a_size << 20, bank_b_size << 20)) {
        LOG_F(ERROR, "Failed to allocate RAM!");
        return -1;
    }

    // find and attach devices to the processor direct slot (PDS)
    // aka video direct slot (VDS)
    setup_pds();

    // Init virtual CPU and request MPC601
    ppc_cpu_init(hmc_obj, PPC_VER::MPC601, true, 7833600ULL);

    return 0;
}

// Monitors supported by the PDM on-board video.
// see displayid.cpp for the full list of supported monitor IDs.
static const std::vector<std::string> PDMBuiltinMonitorIDs = {
    "PortraitGS", "MacRGB12in", "MacRGB15in", "HiRes12-14in", "VGA-SVGA",
    "MacRGB16in", "Multiscan15in", "Multiscan17in", "Multiscan20in",
    "NotConnected"
};

static const PropMap pm6100_settings = {
    {"rambank1_size",
        new IntProperty(0, std::vector<uint32_t>({0, 2, 4, 8, 16, 32, 64, 128}))},
    {"rambank2_size",
        new IntProperty(0, std::vector<uint32_t>({0, 2, 4, 8, 16, 32, 64, 128}))},
    {"mon_id",
        new StrProperty("HiRes12-14in", PDMBuiltinMonitorIDs)},
    {"pds",
        new StrProperty("")},
    {"emmo",
        new BinProperty(0)},
};

static std::vector<std::string> pm6100_devices = {
    "HMC", "Amic"
};

static const DeviceDescription MachinePdm6100_descriptor = {
    MachinePdm::create6100, pm6100_devices, pm6100_settings
};

static const DeviceDescription MachinePdm7100_descriptor = {
    MachinePdm::create7100, pm6100_devices, pm6100_settings
};

static const DeviceDescription MachinePdm8100_descriptor = {
    MachinePdm::create8100, pm6100_devices, pm6100_settings
};

REGISTER_DEVICE(MachinePdm6100, MachinePdm6100_descriptor);
REGISTER_DEVICE(MachinePdm7100, MachinePdm7100_descriptor);
REGISTER_DEVICE(MachinePdm8100, MachinePdm8100_descriptor);

static const MachineDescription pm6100_descriptor = {
    .name = "pm6100",
    .description = "Power Macintosh 6100",
    .machine_root = "MachinePdm6100",
};

static const MachineDescription pm7100_descriptor = {
    .name = "pm7100",
    .description = "Power Macintosh 7100",
    .machine_root = "MachinePdm7100",
};

static const MachineDescription pm8100_descriptor = {
    .name = "pm8100",
    .description = "Power Macintosh 8100",
    .machine_root = "MachinePdm8100",
};

// self-registration with the MachineFactory
REGISTER_MACHINE(pm6100, pm6100_descriptor);
REGISTER_MACHINE(pm7100, pm7100_descriptor);
REGISTER_MACHINE(pm8100, pm8100_descriptor);
