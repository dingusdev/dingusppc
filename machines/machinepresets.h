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

#include <machines/machineproperties.h>

#include <map>

static std::map<std::string, StrProperty> PowerMac6100_Properties = {
    {"machineid",    StrProperty("PowerMacG3")},
    {"cputype",      StrProperty("PPC_MPC750")},
    {"motherboard",  StrProperty("Grackle")},
    {"ioboard",      StrProperty("Heathrow")},
    {"rambank1",     StrProperty("8")},
    {"minram",       StrProperty("32")},
    {"maxram",       StrProperty("256")},
    {"gfxcard",      StrProperty("Nubus_Video")},
    {"gfxmem",       StrProperty("2")}
};

static std::map<std::string, StrProperty> PowerMacG3_Properties = {
    {"machineid",    StrProperty("PowerMacG3")},
    {"cputype",      StrProperty("PPC_MPC750")},
    {"motherboard",  StrProperty("Grackle")},
    {"ioboard",      StrProperty("Heathrow")},
    {"rambank1",     StrProperty("64")},
    {"minram",       StrProperty("32")},
    {"maxram",       StrProperty("256")},
    {"gfxcard",      StrProperty("ATI_Rage_Pro")},
    {"gfxmem",       StrProperty("2")}
};

static std::map<std::string, uint32_t> PPC_CPUs = {
    {"PPC_MPC601",    0x00010001},
    {"PPC_MPC603",    0x00030001},
    {"PPC_MPC604",    0x00040001},
    {"PPC_MPC603E",   0x00060101},
    {"PPC_MPC750",    0x00080200}
};

static std::map<std::string, uint32_t> GFX_CARDs = {
    {"ATI_Rage_Pro",         0x10024750},
    {"ATI_Rage_128_Pro",     0x10025052}
};
