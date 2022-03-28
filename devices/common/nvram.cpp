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

#include <devices/common/hwcomponent.h>
#include <devices/common/nvram.h>

#include <cinttypes>
#include <cstring>
#include <fstream>
#include <iostream>
#include <loguru.hpp>

/** @file Non-volatile RAM implementation.
 */

using namespace std;

/** the signature for NVRAM backing file identification. */
static char NVRAM_FILE_ID[] = "DINGUSPPCNVRAM";

NVram::NVram(std::string file_name, uint32_t ram_size)
{
    this->name = "NVRAM";

    supports_types(HWCompType::NVRAM);

    this->file_name = file_name;
    this->ram_size  = ram_size;

    this->storage = new uint8_t[ram_size];

    this->init();
}

NVram::~NVram() {
    this->save();
    if (this->storage)
        delete this->storage;
}

uint8_t NVram::read_byte(uint32_t offset) {
    return (this->storage[offset]);
}

void NVram::write_byte(uint32_t offset, uint8_t val) {
    this->storage[offset] = val;
}

void NVram::init() {
    char sig[sizeof(NVRAM_FILE_ID)];
    uint16_t data_size;

    ifstream f(this->file_name, ios::in | ios::binary);

    if (f.fail() || !f.read(sig, sizeof(NVRAM_FILE_ID)) ||
        !f.read((char*)&data_size, sizeof(data_size)) ||
        memcmp(sig, NVRAM_FILE_ID, sizeof(NVRAM_FILE_ID)) || data_size != this->ram_size ||
        !f.read((char*)this->storage, this->ram_size)) {
        LOG_F(WARNING, "Could not restore NVRAM content from the given file. \n");
        memset(this->storage, 0, sizeof(this->ram_size));
    }

    f.close();
}

void NVram::save() {
    ofstream f(this->file_name, ios::out | ios::binary);

    /* write file identification */
    f.write(NVRAM_FILE_ID, sizeof(NVRAM_FILE_ID));
    f.write((char*)&this->ram_size, sizeof(this->ram_size));

    /* write NVRAM content */
    f.write((char*)this->storage, this->ram_size);

    f.close();
}
