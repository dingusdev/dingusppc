#include "endianswap.h"
#include <thirdparty/loguru/loguru.hpp>
#include <cinttypes>
#include <string>
#include <map>
#include <memory>
#include <iostream>
#include <utility>

#ifndef MACHINE_PROPERTIES_H
#define MACHINE_PROPERTIES_H

using namespace std;

#define ILLEGAL_DEVICE_VALUE 0x168A523B

/** Property types. */
enum PropType : int {
    PROP_TYPE_UNKNOWN   = 0,
    PROP_TYPE_STRING    = 1,
    PROP_TYPE_INTEGER   = 2,
};

class BasicProperty {
public:
    BasicProperty(PropType type, string val) {
        this->type = type;
        set_string(val);
    }

    virtual ~BasicProperty() = default;

    virtual BasicProperty* clone() const = 0;

    string get_string() {
        return this->val;
    }

    void set_string(string str) {
        this->val = str;
    }

    PropType get_type() {
        return this->type;
    }

protected:
    PropType    type;
    string      val;
};


class StrProperty : public BasicProperty {
public:
    StrProperty(string str)
        : BasicProperty(PROP_TYPE_STRING, str) {}

    BasicProperty* clone() const { return new StrProperty(*this); }

    uint32_t IntRep() {
        try {
            return strtoul(get_string().c_str(), 0, 0);
        } catch (string bad_string) {
            cerr << "Could not convert string " << bad_string << "to an ineteger!" << endl;

            return ILLEGAL_DEVICE_VALUE;
        }
    }
};

class IntProperty : public BasicProperty {
public:
    IntProperty(string str)
        : BasicProperty(PROP_TYPE_INTEGER, str)
    {
        this->int_val   = 0;
        this->min       = std::numeric_limits<uint32_t>::min();
        this->max       = std::numeric_limits<uint32_t>::max();
    }

    IntProperty(string str, uint32_t min, uint32_t max)
        : BasicProperty(PROP_TYPE_INTEGER, str)
    {
        this->int_val   = 0;
        this->min       = min;
        this->max       = max;

        this->int_val = this->get_int();
    }

    BasicProperty* clone() const { return new IntProperty(*this); }

    uint32_t get_int() {
        try {
            uint32_t result = strtoul(this->get_string().c_str(), 0, 0);

            /* perform range check */
            if (result < this->min || result > this->max) {
                LOG_F(ERROR, "Value %d out of range!", result);
            } else {
                this->int_val = result;
            }
        } catch (string bad_string) {
            LOG_F(ERROR, "Could not convert string %s to an integer!",
                bad_string.c_str());
        }
        return this->int_val;
    }

private:
    uint32_t    int_val;
    uint32_t    min;
    uint32_t    max;
};

typedef map<string, BasicProperty*> PropMap;

extern map<string, unique_ptr<BasicProperty>> gMachineSettings;

#define GET_INT_PROP(name) \
    dynamic_cast<IntProperty*>(gMachineSettings.at(name).get())->get_int()

uint32_t get_gfx_card(std::string gfx_str);
uint32_t get_cpu_type(std::string cpu_str);
void search_properties(uint32_t chosen_gestalt);
int establish_machine_presets(
    const char* rom_filepath, std::string machine_str, uint32_t* ram_sizes, uint32_t gfx_mem);

#endif /* MACHINE_PROPERTIES_H */
