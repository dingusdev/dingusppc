#include "machineproperties.h"
#include "machinepresets.h"
#include "machinefactory.h"
#include "devices/memctrlbase.h"
#include <cmath>
#include <fstream>
#include <thirdparty/loguru/loguru.hpp>

uint32_t get_gfx_card(std::string gfx_str) {
    if (gfx_str.compare("Nubus_Video")) {
        return 0;
    } else {
        try {
            return GFX_CARDs.find(gfx_str)->second;
        } catch (std::string bad_string) {
            std::cerr << "Could not find the matching GFX card for " << bad_string << std::endl;

            return ILLEGAL_DEVICE_VALUE;
        }
    }
}

uint32_t get_cpu_type(std::string cpu_str) {
    try {
        return PPC_CPUs.find(cpu_str)->second;
    } catch (std::string bad_string) {
        std::cerr << "Could not find the matching CPU value for " << bad_string << std::endl;

        return ILLEGAL_DEVICE_VALUE;
    }
}

void init_search_array() {
    //Code to generate a vector of maps for machine configs goes here
}

bool is_power_of_two(uint32_t number_to_check) {
    if (number_to_check == 0)
        return true; //This would be false normally, but 0 is used by DPPC to signify an empty bank of RAM

    return (ceil(log2(number_to_check)) == floor(log2(number_to_check)));
}

bool check_ram_size(std::string machine_str, uint32_t number_to_check) {
    uint32_t min_ram = 8;
    uint32_t max_ram = 16;

    if (machine_str.compare("pmg3") == 0) {
        min_ram = PowerMacG3_Properties.find("minram")->second.IntRep();
        max_ram = PowerMacG3_Properties.find("maxram")->second.IntRep();
    }

    if (number_to_check == 0) {
        LOG_F(WARNING, "EMPTY RAM BANK ERROR - size %d", number_to_check);
        return true;
    }

    if ((number_to_check > max_ram) ||
        (number_to_check < min_ram) ||
        !(is_power_of_two(number_to_check))) {
        return false;
    }
    else {
        return true;
    }

}

bool loop_ram_check(std::string machine_str, uint32_t* ram_sizes) {
    for (int checking_stage_one = 0; checking_stage_one < sizeof(ram_sizes); checking_stage_one++) {
        if (checking_stage_one == 0) {
            if (ram_sizes[checking_stage_one] == 0) {
                LOG_F(ERROR, "EMPTY RAM BANK 0!!");
                return false;
            }
        }
        if (check_ram_size(machine_str, ram_sizes[checking_stage_one]) == false) {
            LOG_F(ERROR, "RAM BANK ERROR with RAM BANK %d", checking_stage_one);
            cout << machine_str << endl;
            return false;
        }
    }

    return true;
}

void search_properties(std::string machine_str) {
    std::string cpu_str = "OOPS";
    uint32_t cpu_type   = ILLEGAL_DEVICE_VALUE;
    uint32_t ram_size   = 0;
    uint32_t gfx_size   = 0;
    uint32_t gfx_type   = 0;

    if (machine_str.compare("PowerMacG3") == 0) {
        cpu_str  = PowerMac6100_Properties.find("cputype")->second.get_string();
        cpu_type = get_cpu_type(cpu_str);
        ram_size = PowerMac6100_Properties.find("rambank1")->second.IntRep();
        gfx_size = PowerMac6100_Properties.find("gfxmem")->second.IntRep();
        gfx_type = PowerMac6100_Properties.find("gfxcard")->second.IntRep();
    }
    else if (machine_str.compare("PowerMac6100") == 0) {
        cpu_str = PowerMacG3_Properties.find("cputype")->second.get_string();
        cpu_type = get_cpu_type(cpu_str);
        ram_size = PowerMacG3_Properties.find("rambank1")->second.IntRep();
        gfx_size = PowerMacG3_Properties.find("gfxmem")->second.IntRep();
        gfx_type = PowerMacG3_Properties.find("gfxcard")->second.IntRep();
    }
    else {
        std::cerr << "Unable to find congifuration for " << machine_str << std::endl;
    }

    std::cout << "CPU  TYPE: 0x" << std::hex << cpu_type << std::endl;
    std::cout << "RAM  SIZE: " << std::dec << ram_size << std::endl;
    std::cout << "GMEM SIZE: " << std::dec << gfx_size << std::endl;
}

int establish_machine_presets(
    const char* rom_filepath, std::string machine_str, uint32_t* ram_sizes, uint32_t gfx_mem) {
    ifstream rom_file;
    uint32_t file_size;

    //init_search_array();

    //search_properties(machine_str);

    rom_file.open(rom_filepath, ios::in | ios::binary);
    if (rom_file.fail()) {
        LOG_F(ERROR, "Cound not open the specified ROM file.");
        rom_file.close();
        return -1;
    }


    rom_file.seekg(0, rom_file.end);
    file_size = rom_file.tellg();
    rom_file.seekg(0, rom_file.beg);

    if (file_size != 0x400000UL) {
        LOG_F(ERROR, "Unxpected ROM File size. Expected size is 4 megabytes.");
        rom_file.close();
        return -1;
    }

    if (loop_ram_check(machine_str, ram_sizes) == true) {
        if (machine_str.compare("pmg3") == 0) {
            create_gossamer(ram_sizes, gfx_mem);
        }
    }
    else {
        return -1;
    }

    load_rom(rom_file, file_size);
    rom_file.close();

    return 0;
}
