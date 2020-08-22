
#ifndef MACHINE_PROPERTIES_H
#define MACHINE_PROPERTIES_H

#include "endianswap.h"
#include <cinttypes>
#include <cstring>
#include <map>
#include <iostream>
#include <utility>

using namespace std;

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

            return 0x168A523B;
        }
    }

private:
    string StringInput = "0x168A523B";
};

map<string, uint32_t> PPC_CPUs;

map<string, uint32_t> GFX_CARDs;

enum GESTALT : uint32_t {
    pm6100 = 100,
    pmg3   = 510,
};

void init_ppc_cpu_map();    //JANKY FUNCTION TO FIX VS 2019 BUG!
void init_gpu_map();        //JANKY FUNCTION TO FIX VS 2019 BUG!

uint32_t get_gfx_card(std::string gfx_str);
uint32_t get_cpu_type(std::string cpu_str);
void search_properties(uint32_t chosen_gestalt);

#endif /* MACHINE_PROPERTIES_H */