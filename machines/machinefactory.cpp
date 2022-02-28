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

/** @file Factory for creating different machines.

    Author: Max Poliakovski
 */

#include <devices/memctrl/memctrlbase.h>
#include <loguru.hpp>
#include <machines/machinefactory.h>
#include <machines/machineproperties.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <map>
#include <memory>
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
    {0x476F7373, {"pmg3",     "Power Mac G3 Beige"}},          // Gossamer
    {0x47525820, {"pbg3",     "PowerBook G3 Wallstreet"}},
    //{"Hoop", "PowerBook 3400"},                              // Hooper
    {0x50425820, {"pb-preg3", "PowerBook Pre-G3"}},
    {0x50444D20, {"pm6100",   "Nubus Power Mac"}},             // Piltdown Man (6100/7100/8100)
    {0x50697020, {"pippin",   "Bandai Pippin"}},               // Pippin
    //{"Powe", "Generic Power Mac"},                           // PowerMac?
    {0x544E5420, {"pm7200",   "Power Mac 7xxxx/8xxx series"}}, // Trinitrotoluene :-)
    {0x5A616E7A, {"pm4400",   "Power Mac 4400/7220"}},         // Zanzibar
};

static const vector<string> WriteToggle = {"ON", "on", "OFF", "off"};

static const PropMap GossamerSettings = {
    {"rambank1_size",
        new IntProperty(256, vector<uint32_t>({8, 16, 32, 64, 128, 256}))},
    {"rambank2_size",
        new IntProperty(  0, vector<uint32_t>({0, 8, 16, 32, 64, 128, 256}))},
    {"rambank3_size",
        new IntProperty(  0, vector<uint32_t>({0, 8, 16, 32, 64, 128, 256}))},
    {"gfxmem_size",
        new IntProperty(  2, vector<uint32_t>({2, 4, 6}))},
    {"mon_id",
        new StrProperty("")},
    {"fdd_img",
        new StrProperty("")},
    {"fdd_wr_prot", new StrProperty("OFF", WriteToggle)},
};

/** Monitors supported by the PDM on-board video. */
/* see displayid.cpp for the full list of supported monitor IDs. */
static const vector<string> PDMBuiltinMonitorIDs = {
    "PortraitGS", "MacRGB12in", "MacRGB15in", "HiRes12-14in", "VGA-SVGA",
    "MacRGB16in", "Multiscan15in", "Multiscan17in", "Multiscan20in",
    "NotConnected"
};

static const PropMap PDMSettings = {
    {"rambank1_size",
        new IntProperty(0, vector<uint32_t>({0, 8, 16, 32, 64, 128}))},
    {"rambank2_size",
        new IntProperty(0, vector<uint32_t>({0, 8, 16, 32, 64, 128}))},
    {"mon_id",
        new StrProperty("HiRes12-14in", PDMBuiltinMonitorIDs)},
    {"fdd_img",
        new StrProperty("")},
    {"fdd_wr_prot", 
        new StrProperty("OFF", WriteToggle)},
};

static const map<string, string> PropHelp = {
    {"rambank1_size",   "specifies RAM bank 1 size in MB"},
    {"rambank2_size",   "specifies RAM bank 2 size in MB"},
    {"rambank3_size",   "specifies RAM bank 3 size in MB"},
    {"gfxmem_size",     "specifies video memory size in MB"},
    {"fdd_img",         "specifies path to floppy disk image"},
    {"fdd_wr_prot",     "toggles floppy disks write protection"},
    {"mon_id",          "specifies which monitor to emulate"},
};

static const map<string, tuple<PropMap, function<int(string&)>, string>> machines = {
    {"pm6100", {PDMSettings,      create_pdm,      "PowerMacintosh 6100"}},
    {"pmg3",   {GossamerSettings, create_gossamer, "Power Macintosh G3 (Beige)"}},
};

string machine_name_from_rom(string& rom_filepath) {
    ifstream rom_file;
    uint32_t file_size, config_info_offset, rom_id;
    char rom_id_str[17];

    string machine_name = "";

    rom_file.open(rom_filepath, ios::in | ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Cound not open the specified ROM file.");
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

    LOG_F(INFO, "The machine is identified as... %s\n",
        std::get<1>(rom_identity.at(rom_id)));

    machine_name = std::get<0>(rom_identity.at(rom_id));

bail_out:
    rom_file.close();

    return machine_name;
}

int get_machine_settings(string& id, map<string, string> &settings) {
    try {
        auto props = get<0>(machines.at(id));

        gMachineSettings.clear();

        for (auto& p : props) {
            settings[p.first] = p.second->get_string();

            /* populate dynamic machine settings from presets  */
            gMachineSettings[p.first] = unique_ptr<BasicProperty>(p.second->clone());
        }
    }
    catch(out_of_range ex) {
        LOG_F(ERROR, "Unknown machine id %s", id.c_str());
        return -1;
    }
    return 0;
}

void set_machine_settings(map<string, string> &settings) {
    for (auto& s : settings) {
        gMachineSettings.at(s.first)->set_string(s.second);
    }

    /* print machine settings summary */
    cout << endl << "Machine settings summary: " << endl;

    for (auto& p : gMachineSettings) {
        cout << p.first << " : " << p.second->get_string() << endl;
    }
}

void list_machines() {
    cout << endl << "Supported machines:" << endl << endl;

    for (auto& mach : machines) {
        cout << setw(13) << mach.first << "\t\t" << get<2>(mach.second) << endl;
    }

    cout << endl;
}

void list_properties() {
    cout << endl;

    for (auto& mach : machines) {
        cout << get<2>(mach.second) << " supported properties:" << endl << endl;

        for (auto& p : get<0>(mach.second)) {
            cout << setw(13) << p.first << "\t\t" << PropHelp.at(p.first)
                << endl;

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
            default:
                break;
            }
            cout << endl;
        }
    }

    cout << endl;
}

/* Read ROM file content and transfer it to the dedicated ROM region */
void load_rom(std::ifstream& rom_file, uint32_t file_size) {
    unsigned char* sysrom_mem = new unsigned char[file_size];

    rom_file.seekg(0, ios::beg);
    rom_file.read((char*)sysrom_mem, file_size);

    MemCtrlBase* mem_ctrl = dynamic_cast<MemCtrlBase*>(
        gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    mem_ctrl->set_data(0xFFC00000, sysrom_mem, file_size);
    delete[] sysrom_mem;
}

/* Read ROM file content and transfer it to the dedicated ROM region */
int load_boot_rom(string& rom_filepath) {
    ifstream rom_file;
    size_t   file_size;
    int      result;
    AddressMapEntry *rom_reg;

    rom_file.open(rom_filepath, ios::in | ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Cound not open the specified ROM file.");
        rom_file.close();
        return -1;
    }

    rom_file.seekg(0, rom_file.end);
    file_size = rom_file.tellg();
    rom_file.seekg(0, rom_file.beg);

    if (file_size != 0x400000UL) {
        LOG_F(ERROR, "Unxpected ROM File size. Expected size is 4 megabytes.");
        result = -1;
    } else {
        unsigned char* sysrom_mem = new unsigned char[file_size];

        rom_file.seekg(0, ios::beg);
        rom_file.read((char*)sysrom_mem, file_size);

        MemCtrlBase* mem_ctrl = dynamic_cast<MemCtrlBase*>(
            gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

        if ((rom_reg = mem_ctrl->find_rom_region())) {
            mem_ctrl->set_data(rom_reg->start, sysrom_mem, file_size);
        } else {
            ABORT_F("Could not locate physical ROM region!");
        }

        delete[] sysrom_mem;

        result = 0;
    }

    rom_file.close();

    return result;
}


int create_machine_for_id(string& id, string& rom_filepath) {
    try {
        auto machine = machines.at(id);

        /* build machine and load boot ROM */
        if (get<1>(machine)(id) < 0 || load_boot_rom(rom_filepath) < 0) {
            return -1;
        }
    } catch(out_of_range ex) {
        LOG_F(ERROR, "Unknown machine id %s", id.c_str());
        return -1;
    }

    return 0;
}
