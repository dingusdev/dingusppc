#include "endianswap.h"
#include <cinttypes>
#include <string>
#include <map>
#include <iostream>
#include <utility>

#ifndef MACHINE_PROPERTIES_H
#define MACHINE_PROPERTIES_H
using namespace std;

#define ILLEGAL_DEVICE_VALUE 0x168A523B

class StringProperty {
public:
    StringProperty(string EnterString) {
        StringInput = EnterString;
    }

    string getString() {
        return StringInput;
    }

    uint32_t IntRep() {
        try {
            return strtoul(getString().c_str(), 0, 0);
        } catch (string bad_string) {
            cerr << "Could not convert string " << bad_string << "to an ineteger!" << endl;

            return ILLEGAL_DEVICE_VALUE;
        }
    }

private:
    string StringInput = std::to_string(ILLEGAL_DEVICE_VALUE);
};

void init_ppc_cpu_map();    //JANKY FUNCTION TO FIX VS 2019 BUG!
void init_gpu_map();        //JANKY FUNCTION TO FIX VS 2019 BUG!

uint32_t get_gfx_card(std::string gfx_str);
uint32_t get_cpu_type(std::string cpu_str);
void search_properties(uint32_t chosen_gestalt);
int establish_machine_presets(
    const char* rom_filepath, std::string machine_str, uint32_t* ram_sizes, uint32_t gfx_mem);

#endif /* MACHINE_PROPERTIES_H */