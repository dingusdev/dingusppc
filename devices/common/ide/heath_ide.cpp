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

/** @file Heathrow hard drive controller */

#include "heath_ide.h"
#include <fstream>
#include <limits>
#include <stdio.h>
#include <loguru.hpp>

#define sector_size 512

using namespace std;

HeathIDE::HeathIDE(std::string filename) {
    this->hdd_img;

    // Taken from:
    // https://stackoverflow.com/questions/22984956/tellg-function-give-wrong-size-of-file/22986486
    this->hdd_img.ignore(std::numeric_limits<std::streamsize>::max());
    this->img_size = this->hdd_img.gcount();
    this->hdd_img.clear();    //  Since ignore will have set eof.
    this->hdd_img.seekg(0, std::ios_base::beg);
}


uint32_t HeathIDE::read(int reg) {
    uint32_t res = 0;
    switch (reg) { 
    case HEATH_DATA:
        res = heath_regs[HEATH_DATA];
    case HEATH_ERROR:
        res = heath_regs[HEATH_ERROR];
    case HEATH_SEC_COUNT:
        res = heath_regs[HEATH_SEC_COUNT];
    case HEATH_SEC_NUM:
        res = heath_regs[HEATH_SEC_NUM];
    default:
        LOG_F(WARNING, "Attempted to read unknown IDE register: %x", reg);
    }

    return res;
}


void HeathIDE::write(int reg, uint32_t value) {
    switch (reg) {
    case HEATH_DATA:
        res = heath_regs[HEATH_DATA];
    case HEATH_CMD:
        perform_command(value);
    default:
        LOG_F(WARNING, "Attempted to write unknown IDE register: %x", reg);
    }

void HeathIDE::perform_command(uint32_t command) {
    LOG_F(WARNING, "Attempted to execute IDE command: %x", command);
}