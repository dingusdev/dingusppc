#include <memory>
#include <string>
#include <thirdparty/loguru.hpp>
#include "machinebase.h"

std::unique_ptr<MachineBase> gMachineObj = 0;


MachineBase::MachineBase(std::string name)
{
    this->name = name;

    /* initialize internal maps */
    this->comp_map.clear();
    this->name_to_type.clear();
    this->aliases.clear();
}

MachineBase::~MachineBase()
{
    for(auto it = this->comp_map.begin(); it != this->comp_map.end(); it++) {
        delete it->second;
    }
    this->comp_map.clear();
}

bool MachineBase::add_component(std::string name, HWCompType type, MMIODevice *dev_obj)
{
    if (this->comp_map.count(name)) {
        LOG_F(ERROR, "Component %s already exists!", name.c_str());
        return false;
    }

    this->comp_map[name] = dev_obj;
    this->name_to_type[name] = type;

    return true;
}

void MachineBase::add_alias(std::string name, std::string alias, HWCompType type)
{
    this->aliases[alias]      = name;
    this->name_to_type[alias] = type;
}

MMIODevice *MachineBase::get_comp_by_name(std::string name)
{
    if (this->aliases.count(name)) {
        name = this->aliases[name];
    }

    if (this->comp_map.count(name)) {
        return this->comp_map[name];
    } else {
        return NULL;
    }
}

MMIODevice *MachineBase::get_comp_by_type(HWCompType type)
{
    std::string comp_name;
    bool found = false;

    for(auto it = this->name_to_type.begin(); it != this->name_to_type.end(); it++) {
        if (it->second == type) {
            comp_name = it->first;
            found = true;
            break;
        }
    }

    if (!found)
        return NULL;

    return this->get_comp_by_name(comp_name);
}
