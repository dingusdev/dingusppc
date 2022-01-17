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

#ifndef MACHINE_FACTORY_H
#define MACHINE_FACTORY_H

#include <machines/machinebase.h>

#include <fstream>
#include <string>

using namespace std;

std::string machine_name_from_rom(std::string& rom_filepath);

int  get_machine_settings(string& id, map<string, string> &settings);
void set_machine_settings(map<string, string> &settings);
int  create_machine_for_id(string& id, string& rom_filepath);
void list_machines(void);
void list_properties(void);

/* Machine-specific factory functions. */
int create_catalyst(string& id);
int create_gossamer(string& id);
int create_pdm(string& id);

#endif /* MACHINE_FACTORY_H */
