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

#include <cpu/ppc/ppcemu.h>
#include <devices/common/hwcomponent.h>
#include <devices/common/nvram.h>
#include <devices/common/ofnvram.h>
#include <devices/deviceregistry.h>

#include <cinttypes>
#include <cstring>
#include <fstream>
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

    if (ram_size == 8192)
        this->of_nvram_offset = OF_NVRAM_OFFSET;

    // allocate memory storage and fill it with zeroes
    this->storage = std::unique_ptr<uint8_t[]>(new uint8_t[ram_size] ());

    this->init();
}

NVram::~NVram() {
    this->save();
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
        !f.read((char*)this->storage.get(), this->ram_size)) {
        LOG_F(WARNING, "Could not restore NVRAM content from the given file \"%s\".", this->file_name.c_str());
        memset(this->storage.get(), 0, this->ram_size);
    }

    f.close();
}

void NVram::save() {
    if (is_deterministic) {
        LOG_F(INFO, "Skipping NVRAM write to \"%s\" in deterministic mode", this->file_name.c_str());
        return;
    }
    ofstream f(this->file_name, ios::out | ios::binary);

    /* write file identification */
    f.write(NVRAM_FILE_ID, sizeof(NVRAM_FILE_ID));
    f.write((char*)&this->ram_size, sizeof(this->ram_size));

    /* write NVRAM content */
    f.write((char*)this->storage.get(), this->ram_size);

    f.close();
}

static const DeviceDescription Nvram_Descriptor = {
    NVram::create, {}, {}, HWCompType::NVRAM
};

REGISTER_DEVICE(NVRAM, Nvram_Descriptor);
