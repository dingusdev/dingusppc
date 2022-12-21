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

/** @file Utilities for working with the Apple Open Firmware NVRAM partition. */

#include <devices/common/ofnvram.h>
#include <endianswap.h>
#include <loguru.hpp>
#include <machines/machinebase.h>
#include <memaccess.h>

#include <cinttypes>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <tuple>

using namespace std;

static uint32_t str2env(string& num_str) {
    try {
        return (uint32_t)stoul(num_str, NULL, 0);
    } catch (invalid_argument& exc) {
        try {
            string num_str2 = string("0x") + num_str;
            return std::stoul(num_str2, NULL, 0);
        } catch (invalid_argument& exc) {
            throw invalid_argument(string("Cannot convert ") + num_str);
        }
    }
}

int OfNvramUtils::init()
{
    this->nvram_obj = dynamic_cast<NVram*>(gMachineObj->get_comp_by_name("NVRAM"));
    return this->nvram_obj == nullptr;
}

bool OfNvramUtils::validate()
{
    int         i;
    OfNvramHdr  hdr;

    if (this->nvram_obj == nullptr)
        return false;

    // read OF partition header
    for (i = 0; i < sizeof(OfNvramHdr); i++) {
        ((uint8_t*)&hdr)[i] = this->nvram_obj->read_byte(OF_NVRAM_OFFSET + i);
    }

    // validate partition signature and version
    if (BYTESWAP_16(hdr.sig) != OF_NVRAM_SIG || hdr.version != 5)
        return false;

    this->size = hdr.num_pages * 256;

    if (this->size != 0x800)
        return false;

    // read the entire partition into the local buffer
    for (i = 0; i < this->size; i++) {
        this->buf[i] = this->nvram_obj->read_byte(OF_NVRAM_OFFSET + i);
    }

    // verify partition checksum
    if (this->checksum_partition() ^ 0xFFFFU)
        return false;

    return true;
}

uint16_t OfNvramUtils::checksum_partition()
{
    uint32_t acc = 0;

    for (int i = 0; i < this->size; i += 2) {
        acc += READ_WORD_BE_A(&this->buf[i]);
    }

    return acc + (acc >> 16);
}

void OfNvramUtils::update_partition()
{
    // set checksum in the header to zero
    this->buf[4] = 0;
    this->buf[5] = 0;

    // calculate new checksum
    uint16_t checksum = this->checksum_partition();
    checksum = checksum ? ~checksum : 0xFFFFU;

    // stuff it into the header
    WRITE_WORD_BE_A(&this->buf[4], checksum);

    // write the entire partition back to NVRAM
    for (int i = 0; i < this->size; i++) {
        this->nvram_obj->write_byte(OF_NVRAM_OFFSET + i, this->buf[i]);
    }
}

static const string flag_names[8] = {
    "little-endian?",
    "real-mode?",
    "auto-boot?",
    "diag-switch?",
    "fcode-debug?",
    "oem-banner?",
    "oem-logo?",
    "use-nvramrc?"
};

static const map<string, std::tuple<int, uint16_t>> of_vars = {
    // name,            type,               offset
    {"real-base",       {OF_VAR_TYPE_INT,   0x10}},
    {"real-size",       {OF_VAR_TYPE_INT,   0x14}},
    {"virt-base",       {OF_VAR_TYPE_INT,   0x18}},
    {"virt-size",       {OF_VAR_TYPE_INT,   0x1C}},
    {"load-base",       {OF_VAR_TYPE_INT,   0x20}},
    {"pci-probe-list",  {OF_VAR_TYPE_INT,   0x24}},
    {"screen-#columns", {OF_VAR_TYPE_INT,   0x28}},
    {"screen-#rows",    {OF_VAR_TYPE_INT,   0x2C}},
    {"selftest-#megs",  {OF_VAR_TYPE_INT,   0x30}},
    {"boot-device",     {OF_VAR_TYPE_STR,   0x34}},
    {"boot-file",       {OF_VAR_TYPE_STR,   0x38}},
    {"diag-device",     {OF_VAR_TYPE_STR,   0x3C}},
    {"diag-file",       {OF_VAR_TYPE_STR,   0x40}},
    {"input-device",    {OF_VAR_TYPE_STR,   0x44}},
    {"output-device",   {OF_VAR_TYPE_STR,   0x48}},
    {"oem-banner",      {OF_VAR_TYPE_STR,   0x4C}},
    {"oem-logo",        {OF_VAR_TYPE_STR,   0x50}},
    {"nvramrc",         {OF_VAR_TYPE_STR,   0x54}},
    {"boot-command",    {OF_VAR_TYPE_STR,   0x58}},
};

void OfNvramUtils::printenv()
{
    int i;

    if (!this->validate()) {
        cout << "Invalid Open Firmware partition content!" << endl;
        return;
    }

    uint8_t of_flags = this->buf[12];

    cout << endl;

    // print flags
    for (i = 0; i < 8; i++) {
        cout << setw(20) << left << flag_names[i] <<
            (((of_flags << i) & 0x80) ? "true" : "false") << endl;
    }

    // print the remaining variables
    for (auto& var : of_vars) {
        auto type   = std::get<0>(var.second);
        auto offset = std::get<1>(var.second);

        cout << setw(20) << left << var.first;

        switch (type) {
        case OF_VAR_TYPE_INT:
            cout << hex << READ_DWORD_BE_A(&this->buf[offset]) << endl;
            break;
        case OF_VAR_TYPE_STR:
            uint16_t str_offset = READ_WORD_BE_A(&this->buf[offset]) - OF_NVRAM_OFFSET;
            uint16_t str_len    = READ_WORD_BE_A(&this->buf[offset+2]);
            for (i = 0; i < str_len; i++) {
                char ch = *(char *)&(this->buf[str_offset + i]);
                if (ch == '\x0D') cout << endl; else cout << ch;
            }
            cout << endl;
        }
    }

    cout << endl;
}

void OfNvramUtils::setenv(string var_name, string value)
{
    int i, flag;

    if (!this->validate()) {
        cout << "Invalid Open Firmware partition content!" << endl;
        return;
    }

    // check if the user tries to change a flag
    for (i = 0; i < 8; i++) {
        if (var_name == flag_names[i]) {
            if (value == "true")
                flag = 1;
            else if (value == "false")
                flag = 0;
            else {
                cout << "Invalid property value: " << value << endl;
                return;
            }
            uint8_t flag_bit = 0x80U >> i;
            uint8_t of_flags = this->buf[12];

            if (flag)
                of_flags |= flag_bit;
            else
                of_flags &= ~flag_bit;

            this->buf[12] = of_flags;
            this->update_partition();
            cout << " ok" << endl; // mimic Forth
            return;
        }
    }

    // see if one of the standard properties should be changed
    if (of_vars.find(var_name) == of_vars.end()) {
        cout << "Attempt to change unknown variable " << var_name << endl;
        return;
    }

    auto type   = std::get<0>(of_vars.at(var_name));
    auto offset = std::get<1>(of_vars.at(var_name));

    if (type == OF_VAR_TYPE_INT) {
        uint32_t num;
        try {
            num = str2env(value);
        } catch (invalid_argument& exc) {
            cout << exc.what() << endl;
            return;
        }
        WRITE_DWORD_BE_A(&this->buf[offset], num);
        this->update_partition();
        cout << " ok" << endl; // mimic Forth
    } else {
        uint16_t str_offset = READ_WORD_BE_A(&this->buf[offset]);
        uint16_t str_len    = READ_WORD_BE_A(&this->buf[offset+2]);

        OfNvramHdr *hdr = (OfNvramHdr *)&this->buf[0];
        uint16_t here = READ_WORD_BE_A(&hdr->here);
        uint16_t top  = READ_WORD_BE_A(&hdr->top);

        // check if there is enough space in the heap for the new string
        // the heap is grown down from offset 0x2000 and cannot be lower than here (0x185c)
        uint16_t new_top = top + str_len - value.length();
        if (new_top < here) {
            cout << "No room in the heap!" << endl;
            return;
        }

        // remove the old string
        std::memmove(&this->buf[top + str_len - OF_NVRAM_OFFSET], &this->buf[top - OF_NVRAM_OFFSET], str_offset - top);
        for (auto& var : of_vars) {
            auto type   = std::get<0>(var.second);
            auto offset = std::get<1>(var.second);
            if (type == OF_VAR_TYPE_STR) {
                uint16_t i_str_offset = READ_WORD_BE_A(&this->buf[offset]);
                if (i_str_offset < str_offset) {
                    WRITE_WORD_BE_A(&this->buf[offset], i_str_offset + str_len);
                }
            }
        }
        top = new_top;

        // copy new string into NVRAM buffer char by char
        i = 0;
        for(char& ch : value) {
            this->buf[top + i - OF_NVRAM_OFFSET] = ch == '\x0A' ? '\x0D' : ch;
            i++;
        }

        // stuff new values into the variable state
        WRITE_WORD_BE_A(&this->buf[offset+0], top);
        WRITE_WORD_BE_A(&this->buf[offset+2], value.length());

        // update partition header
        WRITE_WORD_BE_A(&hdr->top, top);

        // update physical NVRAM
        this->update_partition();
        cout << " ok" << endl; // mimic Forth
        return;
    }
}
