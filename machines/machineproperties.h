#include "endianswap.h"
#include <thirdparty/loguru/loguru.hpp>
#include <algorithm>
#include <cinttypes>
#include <string>
#include <map>
#include <memory>
#include <limits> 
#include <sstream>
#include <utility>
#include <vector>

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

/** Check types for property values. */
enum CheckType : int {
    CHECK_TYPE_NONE     = 0,
    CHECK_TYPE_RANGE    = 1,
    CHECK_TYPE_LIST     = 2,
};

/** Abstract base class for properties. */
class BasicProperty {
public:
    BasicProperty(PropType type, string val) {
        this->type = type;
        set_string(val);
    }

    virtual ~BasicProperty() = default;

    /* Clone method for copying derived property objects. */
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


/** Property class that holds a string value. */
class StrProperty : public BasicProperty {
public:
    StrProperty(string str)
        : BasicProperty(PROP_TYPE_STRING, str) {}

    BasicProperty* clone() const { return new StrProperty(*this); }
};

/** Property class that holds an integer value. */
class IntProperty : public BasicProperty {
public:
    /* construct an integer property without value check. */
    IntProperty(uint32_t val)
        : BasicProperty(PROP_TYPE_INTEGER, to_string(val))
    {
        this->int_val    = val;
        this->min        = std::numeric_limits<uint32_t>::min();
        this->max        = std::numeric_limits<uint32_t>::max();
        this->check_type = CHECK_TYPE_NONE;
        this->vec.clear();
    }

    /* construct an integer property with a predefined range. */
    IntProperty(uint32_t val, uint32_t min, uint32_t max)
        : BasicProperty(PROP_TYPE_INTEGER, to_string(val))
    {
        this->int_val    = val;
        this->min        = min;
        this->max        = max;
        this->check_type = CHECK_TYPE_RANGE;
        this->vec.clear();
    }

    /* construct an integer property with a list of valid values. */
    IntProperty(uint32_t val, vector<uint32_t> vec)
        : BasicProperty(PROP_TYPE_INTEGER, to_string(val))
    {
        this->int_val    = val;
        this->min        = std::numeric_limits<uint32_t>::min();
        this->max        = std::numeric_limits<uint32_t>::max();
        this->check_type = CHECK_TYPE_LIST;
        this->vec = vec;
    }

    BasicProperty* clone() const { return new IntProperty(*this); }

    uint32_t get_int() {
        try {
            uint32_t result = strtoul(this->get_string().c_str(), 0, 0);

            /* perform value check */
            if (!this->check_val(result)) {
                LOG_F(ERROR, "Invalid property value %d!", result);
                LOG_F(ERROR, "Valid values: %s!",
                    this->get_valid_values_as_str().c_str());
                this->set_string(to_string(this->int_val));
            } else {
                this->int_val = result;
            }
        } catch (string bad_string) {
            LOG_F(ERROR, "Could not convert string %s to an integer!",
                bad_string.c_str());
        }
        return this->int_val;
    }

    string get_valid_values_as_str() {
        stringstream ss;

        switch (this->check_type) {
        case CHECK_TYPE_RANGE:
            ss << "[" << this->min << "..." << this->max << "]";
            return ss.str();
        case CHECK_TYPE_LIST: {
            bool first = true;
            for (auto it = begin(this->vec); it != end(this->vec); ++it) {
                if (!first)
                    ss << ", ";
                ss << *it;
                first = false;
            }
            return ss.str();
        }
        default:
            return string("None");
        }
    }

protected:
    bool check_val(uint32_t val) {
        switch (this->check_type) {
        case CHECK_TYPE_RANGE:
            if (val < this->min || val > this->max)
                return false;
            else
                return true;
        case CHECK_TYPE_LIST:
            if (find(this->vec.begin(), this->vec.end(), val) != this->vec.end())
                return true;
            else
                return false;
        default:
            return true;
        }
    }

private:
    uint32_t            int_val;
    CheckType           check_type;
    uint32_t            min;
    uint32_t            max;
    vector<uint32_t>    vec;
};

/** Special map type for specifying machine presets. */
typedef map<string, BasicProperty*> PropMap;

/** Global map that holds settings for the running machine. */
extern map<string, unique_ptr<BasicProperty>> gMachineSettings;

/** Conveniency macros to hide complex casts. */
#define GET_STR_PROP(name) \
    dynamic_cast<StrProperty*>(gMachineSettings.at(name).get())->get_string()

#define GET_INT_PROP(name) \
    dynamic_cast<IntProperty*>(gMachineSettings.at(name).get())->get_int()

#endif /* MACHINE_PROPERTIES_H */
