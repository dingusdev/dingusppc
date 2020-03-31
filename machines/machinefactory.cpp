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

/** @file Factory for creating different machines.

    Author: Max Poliakovski
 */

#include <map>
#include <cinttypes>
#include <cstring>
#include <iostream>
#include <fstream>
#include <thirdparty/loguru/loguru.hpp>
#include "memreadwrite.h"
#include "machinefactory.h"
#include "devices/memctrlbase.h"

using namespace std;

/**
    Power Macintosh ROM identification string

    is located in the ConfigInfo structure starting at 0x30D064 (PCI Macs)
    or 0x30C064 (Nubus Macs).
*/
static const map<uint32_t, string> rom_identity = {
    {0x416C6368, "Performa 6400"},                  //Alchemy
    //{"Come", "PowerBook 2400"},                   //Comet
    {0x436F7264, "Power Mac 5200/6200 series"},     //Cordyceps
    {0x47617A65, "Power Mac 6500"},                 //Gazelle
    {0x476F7373, "Power Mac G3 Beige"},             //Gossamer
    {0x47525820, "PowerBook G3 Wallstreet"},
    //{"Hoop", "PowerBook 3400"},                   //Hooper
    {0x50425820, "PowerBook Pre-G3"},
    {0x50444D20, "Nubus Power Mac or WGS"},         //Piltdown Man (6100/7100/8100)
    {0x50697020, "Bandai Pippin"},                  //Pippin
    //{"Powe", "Generic Power Mac"},                //PowerMac?
    //{"Spar", "20th Anniversay Mac"},              //Spartacus
    {0x544E5420, "Power Mac 7xxxx/8xxx series"},    //Trinitrotoluene :-)
    {0x5A616E7A, "Power Mac 4400/7220"},            //Zanzibar
    //{"????", "A clone, perhaps?"}                 //N/A (Placeholder ID)
};


int create_machine_for_id(uint32_t id)
{
    switch(id) {
        case 0x476F7373:
            create_gossamer();
            break;
        default:
            LOG_F(ERROR, "Unknown machine ID: %X", id);
            return -1;
    }
    return 0;
}


/* Read ROM file content and transfer it to the dedicated ROM region */
void load_rom(ifstream& rom_file, uint32_t file_size)
{
    unsigned char *sysrom_mem = new unsigned char[file_size];

    rom_file.seekg(0, ios::beg);
    rom_file.read((char *)sysrom_mem, file_size);

    MemCtrlBase *mem_ctrl = dynamic_cast<MemCtrlBase *>
        (gMachineObj->get_comp_by_type(HWCompType::MEM_CTRL));

    mem_ctrl->set_data(0xFFC00000, sysrom_mem, file_size);
    delete[] sysrom_mem;
}


int create_machine_for_rom(const char* rom_filepath)
{
    ifstream rom_file;
    uint32_t file_size, config_info_offset, rom_id;
    char rom_id_str[17];

    rom_file.open(rom_filepath, ios::in|ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Cound not open the specified ROM file.");
        rom_file.close();
        return -1;
    }

    rom_file.seekg(0, rom_file.end);
    file_size = rom_file.tellg();
    rom_file.seekg(0, rom_file.beg);

    if (file_size != 0x400000UL){
        LOG_F(ERROR, "Unxpected ROM File size. Expected size is 4 megabytes.");
        rom_file.close();
        return -1;
    }

    /* read config info offset from file */
    config_info_offset = 0;
    rom_file.seekg(0x300080, ios::beg);
    rom_file.read((char *)&config_info_offset, 4);
    config_info_offset = READ_DWORD_BE_A(&config_info_offset);

    /* rewind to ConfigInfo.BootstrapVersion field */
    rom_file.seekg(0x300064 + config_info_offset, ios::beg);

    /* read BootstrapVersion as C string */
    rom_file.read(rom_id_str, 16);
    rom_id_str[16] = 0;
    LOG_F(INFO, "ROM BootstrapVersion: %s", rom_id_str);

    if (strncmp(rom_id_str, "Boot", 4) != 0) {
        LOG_F(ERROR, "Invalid BootstrapVersion string.");
        rom_file.close();
        return -1;
    }

    /* convert BootstrapVersion string to ROM ID */
    rom_id = (rom_id_str[5] << 24) | (rom_id_str[6] << 16) |
             (rom_id_str[7] <<  8) | rom_id_str[8];

    LOG_F(INFO, "The machine is identified as... %s\n", rom_identity.at(rom_id).c_str());

    create_machine_for_id(rom_id);

    load_rom(rom_file, file_size);

    rom_file.close();
    return 0;
}
