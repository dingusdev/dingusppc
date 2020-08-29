#include "machineproperties.h"
#include "machinepresets.h"
#include <cmath>
#include <thirdparty/loguru/loguru.hpp>

void init_ppc_cpu_map() {

    PPC_CPUs.emplace("PPC_MPC601",  0x00010001);
    PPC_CPUs.emplace("PPC_MPC603",  0x00030001);
    PPC_CPUs.emplace("PPC_MPC604",  0x00040001);
    PPC_CPUs.emplace("PPC_MPC603E", 0x00060101);
    PPC_CPUs.emplace("PPC_MPC750",  0x00080200);
}

void init_gpu_map() {
    GFX_CARDs.emplace("ATI_Rage_Pro",     0x10024750);
    GFX_CARDs.emplace("ATI_Rage_128_Pro", 0x10025052);
}

void init_machine_properties() {
    PowerMac6100_Properties.emplace("machineid",   StringProperty("PowerMac6100"));
    PowerMac6100_Properties.emplace("cputype",     StringProperty("PPC_MPC601"));
    PowerMac6100_Properties.emplace("motherboard", StringProperty("Nubus"));
    PowerMac6100_Properties.emplace("ioboard",     StringProperty("Nubus_IO"));
    PowerMac6100_Properties.emplace("rambank1",    StringProperty("8"));
    PowerMac6100_Properties.emplace("gfxcard",     StringProperty("Nubus_Video"));
    PowerMac6100_Properties.emplace("gfxmem",      StringProperty("1"));
    PowerMac6100_Properties.emplace("minram",      StringProperty("8"));
    PowerMac6100_Properties.emplace("maxram",      StringProperty("136"));

    PowerMacG3_Properties.emplace("machineid",   StringProperty("PowerMacG3"));
    PowerMacG3_Properties.emplace("cputype",     StringProperty("PPC_MPC750"));
    PowerMacG3_Properties.emplace("motherboard", StringProperty("Grackle"));
    PowerMacG3_Properties.emplace("ioboard",     StringProperty("Heathrow"));
    PowerMacG3_Properties.emplace("rambank1",    StringProperty("64"));
    PowerMacG3_Properties.emplace("gfxcard",     StringProperty("ATI_Rage_Pro"));
    PowerMacG3_Properties.emplace("gfxmem",      StringProperty("2"));
    PowerMacG3_Properties.emplace("minram",      StringProperty("32"));
    PowerMacG3_Properties.emplace("maxram",      StringProperty("256"));
}

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
        return false;

    return (ceil(log2(number_to_check)) == floor(log2(number_to_check)));
}

bool check_ram_size(std::string machine_str, uint32_t number_to_check) {
    uint32_t min_ram = 8;
    uint32_t max_ram = 16;

    if (machine_str.compare("PowerMacG3") == 0) {
        min_ram = PowerMacG3_Properties.find("minram")->second.IntRep();
        max_ram = PowerMacG3_Properties.find("maxram")->second.IntRep();
    }

    if ((number_to_check > max_ram) || 
        (number_to_check > max_ram) || 
        !(is_power_of_two(number_to_check))) {
        return false;
    } 
    else {
        return true;
    }

}

bool loop_ram_check(std::string machine_str, uint32_t* ram_sizes) {
    for (int checking_stage_one = 0; checking_stage_one < sizeof(ram_sizes); checking_stage_one++) {
        if (check_ram_size(machine_str, ram_sizes[checking_stage_one]) == false) {
            LOG_F(ERROR, "RAM BANK ERROR with RAM BANK %d", checking_stage_one);
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
        cpu_str  = PowerMac6100_Properties.find("cputype")->second.getString();
        cpu_type = get_cpu_type(cpu_str);
        ram_size = PowerMac6100_Properties.find("rambank1")->second.IntRep();
        gfx_size = PowerMac6100_Properties.find("gfxmem")->second.IntRep();
        gfx_type = PowerMac6100_Properties.find("gfxcard")->second.IntRep();
    }
    else if (machine_str.compare("PowerMac6100") == 0) {
        cpu_str = PowerMacG3_Properties.find("cputype")->second.getString();
        cpu_type = get_cpu_type(cpu_str);
        ram_size = PowerMacG3_Properties.find("rambank1")->second.IntRep();
        gfx_size = PowerMacG3_Properties.find("gfxmem")->second.IntRep();
        gfx_type = PowerMacG3_Properties.find("gfxcard")->second.IntRep();
    }
    else {
        std::cerr << "Unable to find congifuration for " << machine_str << std::endl;
    }


    uint16_t gfx_ven = gfx_type >> 16;
    uint16_t gfx_dev = gfx_type & 0xFFFF;

    std::cout << "CPU  TYPE: 0x" << std::hex << cpu_type << std::endl;
    std::cout << "RAM  SIZE: " << std::dec << ram_size << std::endl;
    std::cout << "GMEM SIZE: " << std::dec << gfx_size << std::endl;
}

bool establish_machine_settings(std::string machine_str, uint32_t* ram_sizes) {
    init_ppc_cpu_map();
    init_gpu_map();
    init_machine_properties();
    //init_search_array();

    //search_properties(machine_str);

    if (loop_ram_check (machine_str, ram_sizes)) {
        return true;
    } 
    else {
        return false;
    }
}