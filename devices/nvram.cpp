//DingusPPC
//Written by divingkatae and maximum
//(c)2018-20 (theweirdo)     spatium
//Please ask for permission
//if you want to distribute this.
//(divingkatae#1017 or powermax#2286 on Discord)

#include <iostream>
#include <fstream>
#include <cinttypes>
#include "nvram.h"

using namespace std;


NVram::NVram()
{
    this->nvram_init();
}

NVram::~NVram()
{
    this->nvram_save();
}

void NVram::nvram_init() {
    NVram::nvram_file.open("nvram.bin", ios::in | ios::out | ios::binary);

    if (nvram_file.fail()) {
        std::cout << "Warning: Could not find the NVRAM file. This will be blank. \n" << endl;
        NVram::nvram_file.write(0x00, 8192);
        return;
    }

    NVram::nvram_file.seekg(0, nvram_file.end);
    NVram::nvram_filesize = nvram_file.tellg();
    NVram::nvram_file.seekg(0, nvram_file.beg);

    if (NVram::nvram_filesize != 0x2000) {
        NVram::nvram_file.write(0x00, 8192);
    }

    char temp_storage[8192];

    NVram::nvram_file.read(temp_storage, 8192);

    //hack copying to the NVRAM Array GO!
    for (int i = 0; i < 8192; i++)
    {
        NVram::nvram_storage[i] = (uint8_t) temp_storage[i];
    }
}

void NVram::nvram_save() {
    std::ofstream outfile("nvram.bin");

    outfile.write((char*)nvram_storage, 8192);

    outfile.close();
}

uint8_t NVram::nvram_read(uint32_t nvram_offset){
    nvram_value = NVram::nvram_storage[nvram_offset];
    return nvram_value;
}

void NVram::nvram_write(uint32_t nvram_offset, uint8_t write_val){
    NVram::nvram_storage[nvram_offset] = write_val;
}