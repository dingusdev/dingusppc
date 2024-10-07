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

/** @file Factory for creating different machines.

    Author: Max Poliakovski
 */

#include <devices/common/hwcomponent.h>
#include <devices/deviceregistry.h>
#include <devices/memctrl/memctrlbase.h>
#include <devices/sound/soundserver.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <fstream>
#include <tuple>
#include <iostream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>

using namespace std;

map<string, unique_ptr<BasicProperty>> gMachineSettings;

/**
    Power Macintosh ROM identification map.

    Maps Bootstrap string located at offset 0x30D064 (PCI Macs)
    or 0x30C064 (Nubus Macs) to machine name and description.
*/
static const map<uint32_t, std::tuple<string, const char*>> rom_identity = {
    {0x416C6368, {"pm6400",   "Performa 6400"}},               // Alchemy
    //{"Come", "PowerBook 2400"},                              // Comet
    {0x436F7264, {"pm5200",   "Power Mac 5200/6200 series"}},  // Cordyceps
    {0x47617A65, {"pm6500",   "Power Mac 6500"}},              // Gazelle
    {0x476F7373, {"pmg3dt",   "Power Mac G3 Beige"}},          // Gossamer
    {0x47525820, {"pbg3",     "PowerBook G3 Wallstreet"}},
    //{"Hoop", "PowerBook 3400"},                              // Hooper
    {0x50425820, {"pb-preg3", "PowerBook Pre-G3"}},
    {0x50444D20, {"pm6100",   "Nubus Power Mac"}},             // Piltdown Man (6100/7100/8100)
    {0x50697020, {"pippin",   "Bandai Pippin"}},               // Pippin
    //{"Powe", "Generic Power Mac"},                           // Power Mac?
    {0x544E5420, {"pm7200",   "Power Mac 7xxxx/8xxx series"}}, // Trinitrotoluene :-)
    {0x5A616E7A, {"pm4400",   "Power Mac 4400/7220"}},         // Zanzibar
};

static const map<string, string> PropHelp = {
    {"rambank1_size",   "specifies RAM bank 1 size in MB"},
    {"rambank2_size",   "specifies RAM bank 2 size in MB"},
    {"rambank3_size",   "specifies RAM bank 3 size in MB"},
    {"rambank4_size",   "specifies RAM bank 4 size in MB"},
    {"gfxmem_size",     "specifies video memory size in MB"},
    {"fdd_img",         "specifies path to floppy disk image"},
    {"fdd_fmt",         "specifies floppy disk format"},
    {"fdd_wr_prot",     "toggles floppy disk's write protection"},
    {"hdd_img",         "specifies path(s) to hard disk image(s)"},
    {"hdd_img2",        "specifies path(s) to secondary hard disk image(s)"},
    {"cdr_config",      "CD-ROM device path in [bus]:[device#] format"},
    {"cdr_img",         "specifies path(s) to CD-ROM image(s)"},
    {"cdr_img2",        "specifies path(s) to secondary CD-ROM image(s)"},
    {"mon_id",          "specifies which monitor to emulate"},
    {"pci_PERCH",       "insert a PCI device into PERCH slot"},
    {"pci_A1",          "insert a PCI device into A1 slot"},
    {"pci_B1",          "insert a PCI device into B1 slot"},
    {"pci_C1",          "insert a PCI device into C1 slot"},
    {"pci_D2",          "insert a PCI device into D2 slot"},
    {"pci_E2",          "insert a PCI device into E2 slot"},
    {"pci_F2",          "insert a PCI device into F2 slot"},
    {"vci_D",           "insert a VCI device 0x0D"},
    {"vci_E",           "insert a VCI device 0x0E"},
    {"vci_F",           "insert a VCI device 0x0F"},
    {"serial_backend",  "specifies the backend for the serial port"},
    {"emmo",            "enables/disables factory HW tests during startup"},
    {"cpu",             "specifies CPU"},
    {"adb_devices",     "specifies which ADB device(s) to attach"},
};

bool MachineFactory::add(const string& machine_id, MachineDescription desc)
{
    if (get_registry().find(machine_id) != get_registry().end()) {
        return false;
    }

    get_registry()[machine_id] = desc;
    return true;
}

void MachineFactory::list_machines()
{
    cout << endl << "Supported machines:" << endl << endl;

    for (auto& m : get_registry()) {
        cout << setw(13) << m.first << "\t\t" << m.second.description << endl;
    }

    cout << endl;
}

void MachineFactory::create_device(string& dev_name, DeviceDescription& dev)
{
    for (auto& subdev_name : dev.subdev_list) {
        create_device(subdev_name, DeviceRegistry::get_descriptor(subdev_name));
    }

    gMachineObj->add_device(dev_name, dev.m_create_func());
}

int MachineFactory::create(string& mach_id)
{
    auto it = get_registry().find(mach_id);
    if (it == get_registry().end()) {
        LOG_F(ERROR, "Unknown machine id %s", mach_id.c_str());
        return -1;
    }

    LOG_F(INFO, "Initializing %s hardware...", it->second.description.c_str());

    // initialize global machine object
    gMachineObj.reset(new MachineBase(it->second.name));

    // create and register sound server
    gMachineObj->add_device("SoundServer", std::unique_ptr<SoundServer>(new SoundServer()));

    // recursively create device objects
    for (auto& dev_name : it->second.devices) {
        create_device(dev_name, DeviceRegistry::get_descriptor(dev_name));
    }

    if (it->second.init_func(mach_id)) {
        LOG_F(ERROR, "Machine initialization function failed!");
        return -1;
    }

    // post-initialize all devices
    if (gMachineObj->postinit_devices()) {
        LOG_F(ERROR, "Could not post-initialize devices!");
        return -1;
    }

    LOG_F(INFO, "Initialization completed.");

    return 0;
}

void MachineFactory::list_properties()
{
    cout << endl;

    for (auto& mach : get_registry()) {
        cout << mach.second.description << " supported properties:" << endl << endl;

        print_settings(mach.second.settings);

        for (auto& d : mach.second.devices) {
            list_device_settings(DeviceRegistry::get_descriptor(d));
        }
    }

    cout << endl;
}

void MachineFactory::list_device_settings(DeviceDescription& dev)
{
    for (auto& d : dev.subdev_list) {
        list_device_settings(DeviceRegistry::get_descriptor(d));
    }

    print_settings(dev.properties);
}

void MachineFactory::print_settings(PropMap& prop_map)
{
    string help;

    for (auto& p : prop_map) {
        if (PropHelp.find(p.first) != PropHelp.end())
            help = PropHelp.at(p.first);
        else
            help = "";

        cout << setw(13) << p.first << "\t\t" << help << endl;

        cout << setw(13) << "\t\t\t" "Valid values: ";

        switch(p.second->get_type()) {
        case PROP_TYPE_INTEGER:
            cout << dynamic_cast<IntProperty*>(p.second)->get_valid_values_as_str()
                << endl;
            break;
        case PROP_TYPE_STRING:
            cout << dynamic_cast<StrProperty*>(p.second)->get_valid_values_as_str()
                << endl;
            break;
        case PROP_TYPE_BINARY:
            cout << dynamic_cast<BinProperty*>(p.second)->get_valid_values_as_str()
                << endl;
            break;
        default:
            break;
        }
        cout << endl;
    }
}

void MachineFactory::get_device_settings(DeviceDescription& dev, map<string, string> &settings)
{
    for (auto& d : dev.subdev_list) {
        get_device_settings(DeviceRegistry::get_descriptor(d), settings);
    }

    for (auto& p : dev.properties) {
        if (settings.count(p.first)) {
            // This is a hack. Need to implement hierarchical paths and per device properties.
            LOG_F(ERROR, "Duplicate setting \"%s\".", p.first.c_str());
        }
        else {
            settings[p.first] = p.second->get_string();

            // populate dynamic machine settings from presets
            gMachineSettings[p.first] = unique_ptr<BasicProperty>(p.second->clone());
        }
    }
}

int MachineFactory::get_machine_settings(const string& id, map<string, string> &settings)
{
    auto it = get_registry().find(id);
    if (it != get_registry().end()) {
        auto props = it->second.settings;

        gMachineSettings.clear();

        for (auto& p : props) {
            settings[p.first] = p.second->get_string();

            // populate dynamic machine settings from presets
            gMachineSettings[p.first] = unique_ptr<BasicProperty>(p.second->clone());
        }

        for (auto& dev : it->second.devices) {
            get_device_settings(DeviceRegistry::get_descriptor(dev), settings);
        }
    } else {
        LOG_F(ERROR, "Unknown machine id %s", id.c_str());
        return -1;
    }

    return 0;
}

void MachineFactory::set_machine_settings(map<string, string> &settings) {
    for (auto& s : settings) {
        gMachineSettings.at(s.first)->set_string(s.second);
    }

    // print machine settings summary
    cout << endl << "Machine settings summary: " << endl;

    for (auto& p : gMachineSettings) {
        cout << p.first << " : " << p.second->get_string() << endl;
    }
}

string MachineFactory::machine_name_from_rom(string& rom_filepath) {
    ifstream rom_file;
    size_t file_size;
    uint32_t config_info_offset, rom_id;
    char rom_id_str[17];

    string machine_name = "";

    rom_file.open(rom_filepath, ios::in | ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Could not open the specified ROM file.");
        goto bail_out;
    }

    rom_file.seekg(0, rom_file.end);
    file_size = rom_file.tellg();
    rom_file.seekg(0, rom_file.beg);

    if (file_size != 0x400000UL) {
        LOG_F(ERROR, "Unxpected ROM File size. Expected size is 4 megabytes.");
        goto bail_out;
    }

    /* read config info offset from file */
    config_info_offset = 0;
    rom_file.seekg(0x300080, ios::beg);
    rom_file.read((char*)&config_info_offset, 4);
    config_info_offset = READ_DWORD_BE_A(&config_info_offset);

    /* rewind to ConfigInfo.BootstrapVersion field */
    rom_file.seekg(0x300064 + config_info_offset, ios::beg);

    /* read BootstrapVersion as C string */
    rom_file.read(rom_id_str, 16);
    rom_id_str[16] = 0;
    LOG_F(INFO, "ROM BootstrapVersion: %s", rom_id_str);

    if (strncmp(rom_id_str, "Boot", 4) != 0) {
        LOG_F(ERROR, "Invalid BootstrapVersion string.");
        goto bail_out;
    }

    /* convert BootstrapVersion string to ROM ID */
    rom_id = (rom_id_str[5] << 24) | (rom_id_str[6] << 16) |
        (rom_id_str[7] << 8) | rom_id_str[8];

    LOG_F(INFO, "The machine is identified as... %s",
        std::get<1>(rom_identity.at(rom_id)));

    machine_name = std::get<0>(rom_identity.at(rom_id));

bail_out:
    rom_file.close();

    return machine_name;
}

/* Read ROM file content and transfer it to the dedicated ROM region */
int MachineFactory::load_boot_rom(string& rom_filepath) {
    ifstream rom_file;
    size_t   file_size;
    int      result = 0;
    uint32_t rom_load_addr;
    //AddressMapEntry *rom_reg;

    rom_file.open(rom_filepath, ios::in | ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Could not open the specified ROM file.");
        rom_file.close();
        return -1;
    }

    rom_file.seekg(0, rom_file.end);
    file_size = rom_file.tellg();
    rom_file.seekg(0, rom_file.beg);

    if (file_size == 0x400000UL) { // Old World ROMs
        rom_load_addr = 0xFFC00000UL;
    } else if (file_size == 0x100000UL) { // New World ROMs
        rom_load_addr = 0xFFF00000UL;
    } else {
        LOG_F(ERROR, "Unxpected ROM File size: %zu bytes.", file_size);
        result = -1;
    }

    if (!result) {
        unsigned char* sysrom_mem = new unsigned char[file_size];

        rom_file.seekg(0, ios::beg);
        rom_file.read((char*)sysrom_mem, file_size);

        MemCtrlBase* mem_ctrl = dynamic_cast<MemCtrlBase*>(
            gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

        if ((/*rom_reg = */mem_ctrl->find_rom_region())) {
            mem_ctrl->set_data(rom_load_addr, sysrom_mem, (uint32_t)file_size);
        } else {
            LOG_F(ERROR, "Could not locate physical ROM region!");
            result = -1;
        }

        delete[] sysrom_mem;
    }

    rom_file.close();

    return result;
}

int MachineFactory::create_machine_for_id(string& id, string& rom_filepath) {
    if (MachineFactory::create(id) < 0 || load_boot_rom(rom_filepath) < 0) {
        return -1;
    }

    return 0;
}
