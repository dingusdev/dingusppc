//DingusPPC
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 or powermax#2286 on Discord)

#include <iostream>
#include <fstream>
#include <cstring>
#include <cinttypes>
#include "nvram.h"

/** @file Non-volatile RAM implementation.
 */

using namespace std;

/** the signature for NVRAM backing file identification. */
static char NVRAM_FILE_ID[] = "DINGUSPPCNVRAM";

NVram::NVram(std::string file_name, uint32_t ram_size)
{
    this->file_name = file_name;
    this->ram_size  = ram_size;

    this->storage = new uint8_t[ram_size];

    this->init();
}

NVram::~NVram()
{
    this->save();
    if (this->storage)
        delete this->storage;
}

uint8_t NVram::read_byte(uint32_t offset)
{
    return (this->storage[offset]);
}

void NVram::write_byte(uint32_t offset, uint8_t val)
{
    this->storage[offset] = val;
}

void NVram::init() {
    char     sig[sizeof(NVRAM_FILE_ID)];
    uint16_t data_size;

    ifstream f(this->file_name, ios::in | ios::binary);

    if (f.fail() || !f.read(sig, sizeof(NVRAM_FILE_ID))   ||
        !f.read((char *)&data_size, sizeof(data_size))    ||
        memcmp(sig, NVRAM_FILE_ID, sizeof(NVRAM_FILE_ID)) ||
        data_size != this->ram_size                       ||
        !f.read((char *)this->storage, this->ram_size))
    {
        cout << "WARN: Could not restore NVRAM content from the given file." << endl;
        memset(this->storage, 0, sizeof(this->ram_size));
    }

    f.close();
}

void NVram::save()
{
    ofstream f(this->file_name, ios::out | ios::binary);

    /* write file identification */
    f.write(NVRAM_FILE_ID, sizeof(NVRAM_FILE_ID));
    f.write((char *)&this->ram_size, sizeof(this->ram_size));

    /* write NVRAM content */
    f.write((char *)this->storage, this->ram_size);

    f.close();
}
