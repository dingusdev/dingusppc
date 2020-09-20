#include "endianswap.h"
#include <thirdparty/loguru/loguru.hpp>
#include <cinttypes>
#include <string>
#include <map>
#include <iostream>
#include <utility>

#ifndef MACHINE_PROPERTIES_H
#define MACHINE_PROPERTIES_H
using namespace std;

#define ILLEGAL_DEVICE_VALUE 0x168A523B

class StrProperty {
public:
    StrProperty(string str) {
        this->prop_val = str;
    }

    string get_string() {
        return this->prop_val;
    }

    void set_string(string str) {
        this->prop_val = str;
    }

    uint32_t IntRep() {
        try {
            return strtoul(get_string().c_str(), 0, 0);
        } catch (string bad_string) {
            cerr << "Could not convert string " << bad_string << "to an ineteger!" << endl;

            return ILLEGAL_DEVICE_VALUE;
        }
    }

private:
    string prop_val = string("");
};

class IntProperty {
public:
    IntProperty(string str) {
        this->prop_val = str;
    }

    void set_string(string str) {
        this->prop_val = str;
    }

    uint32_t get_int() {
        try {
            return strtoul(this->prop_val.c_str(), 0, 0);
        } catch (string bad_string) {
            LOG_F(ERROR, "Could not convert string %s to an integer!",
                bad_string.c_str());
        }
        return 0;
    }

private:
    string prop_val = string("");
};

uint32_t get_gfx_card(std::string gfx_str);
uint32_t get_cpu_type(std::string cpu_str);
void search_properties(uint32_t chosen_gestalt);
int establish_machine_presets(
    const char* rom_filepath, std::string machine_str, uint32_t* ram_sizes, uint32_t gfx_mem);

#endif /* MACHINE_PROPERTIES_H */
