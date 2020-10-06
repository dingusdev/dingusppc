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

#ifndef MACHINE_FACTORY_H
#define MACHINE_FACTORY_H

#include "machinebase.h"
#include <fstream>
#include <string>

using namespace std;

std::string machine_name_from_rom(std::string& rom_filepath);

void load_rom(std::ifstream& rom_file, uint32_t file_size);

int  get_machine_settings(string& id, map<string, string> &settings);
void set_machine_settings(map<string, string> &settings);
int  create_machine_for_id(string& id, string& rom_filepath);

int create_machine_for_rom(const char* rom_filepath, uint32_t* grab_ram_size, uint32_t gfx_size);

/* Machine-specific factory functions. */
int create_gossamer(void);

#endif /* MACHINE_FACTORY_H */
