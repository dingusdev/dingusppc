#include "machineproperties.h"

std::map<std::string, StringProperty> PowerMac6100_Properties;

std::map<std::string, StringProperty> PowerMacG3_Properties;

map<string, uint32_t> PPC_CPUs;

map<string, uint32_t> GFX_CARDs;

void init_machine_properties();    // JANKY FUNCTION TO FIX VS 2019 BUG!