#include "machineproperties.h"
#include "machinepresets.h"

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
    PowerMac6100_Properties.emplace("machineid",   StringProperty("Nubus Power Mac"));
    PowerMac6100_Properties.emplace("cputype",     StringProperty("PPC_MPC601"));
    PowerMac6100_Properties.emplace("motherboard", StringProperty("Nubus"));
    PowerMac6100_Properties.emplace("ioboard",     StringProperty("Nubus_IO"));
    PowerMac6100_Properties.emplace("ram",         StringProperty("16"));
    PowerMac6100_Properties.emplace("gfxcard",     StringProperty("Nubus_Video"));
    PowerMac6100_Properties.emplace("gfxmem",      StringProperty("1"));

     
    PowerMacG3_Properties.emplace("machineid",   StringProperty("Power Mac G3 Beige"));
    PowerMacG3_Properties.emplace("cputype",     StringProperty("PPC_MPC750"));
    PowerMacG3_Properties.emplace("motherboard", StringProperty("Grackle"));
    PowerMacG3_Properties.emplace("ioboard",     StringProperty("Heathrow"));
    PowerMacG3_Properties.emplace("ram",         StringProperty("64"));
    PowerMacG3_Properties.emplace("gfxcard",     StringProperty("ATI_Rage_Pro"));
    PowerMacG3_Properties.emplace("gfxmem",      StringProperty("2"));
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

void search_properties(uint32_t chosen_gestalt) {
    std::string cpu_str = "OOPS";
    uint32_t cpu_type   = ILLEGAL_DEVICE_VALUE;
    uint32_t ram_size   = 0;
    uint32_t gfx_size   = 0;
    uint32_t gfx_type   = 0;

    switch (chosen_gestalt) {
    case 100:
        cpu_str  = PowerMac6100_Properties.find("cputype")->second.getString();
        cpu_type = get_cpu_type(cpu_str);
        ram_size = PowerMac6100_Properties.find("ram")->second.IntRep();
        gfx_size = PowerMac6100_Properties.find("gfxmem")->second.IntRep();
        gfx_type = PowerMac6100_Properties.find("gfxcard")->second.IntRep();
        break;
    case 510:
        cpu_str  = PowerMacG3_Properties.find("cputype")->second.getString();
        cpu_type = get_cpu_type(cpu_str);
        ram_size = PowerMacG3_Properties.find("ram")->second.IntRep();
        gfx_size = PowerMacG3_Properties.find("gfxmem")->second.IntRep();
        gfx_type = PowerMacG3_Properties.find("gfxcard")->second.IntRep();
        break;
    default:
        std::cerr << "Unable to find congifuration for " << chosen_gestalt << std::endl;
    }


    uint16_t gfx_ven = gfx_type >> 16;
    uint16_t gfx_dev = gfx_type & 0xFFFF;

    std::cout << "CPU  TYPE: 0x" << std::hex << cpu_type << std::endl;
    std::cout << "RAM  SIZE: " << std::dec << ram_size << std::endl;
    std::cout << "GMEM SIZE: " << std::dec << gfx_size << std::endl;
}

void establish_machine_settings(uint32_t starting_setting) {
    init_ppc_cpu_map();
    init_gpu_map();
    init_machine_properties();

    search_properties(starting_setting);
}