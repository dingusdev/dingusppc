#include "machineproperties.h"
#include <map>

static std::map<std::string, StringProperty> PowerMac6100_Properties = {
    {"machineid",    StringProperty("PowerMacG3")},
    {"cputype",      StringProperty("PPC_MPC750")},
    {"motherboard",  StringProperty("Grackle")},
    {"ioboard",      StringProperty("Heathrow")},
    {"rambank1",     StringProperty("64")},
    {"minram",       StringProperty("32")},
    {"maxram",       StringProperty("256")},
    {"gfxcard",      StringProperty("Nubus_Video")},
    {"gfxmem",       StringProperty("2")}
};

static std::map<std::string, StringProperty> PowerMacG3_Properties = {
    {"machineid",    StringProperty("PowerMacG3")},
    {"cputype",      StringProperty("PPC_MPC750")},
    {"motherboard",  StringProperty("Grackle")},
    {"ioboard",      StringProperty("Heathrow")},
    {"rambank1",     StringProperty("64")},
    {"minram",       StringProperty("32")},
    {"maxram",       StringProperty("256")},
    {"gfxcard",      StringProperty("ATI_Rage_Pro")},
    {"gfxmem",       StringProperty("2")}
};

static std::map<std::string, uint32_t> PPC_CPUs = {
    {"PPC_MPC601",    0x00010001},
    {"PPC_MPC603",    0x00030001},
    {"PPC_MPC604",    0x00040001},
    {"PPC_MPC603E",   0x00060101},
    {"PPC_MPC750",    0x00080200}
};

static std::map<std::string, uint32_t> GFX_CARDs = {
    {"ATI_Rage_Pro",         0x10024750},
    {"ATI_Rage_128_Pro",     0x10025052}
};