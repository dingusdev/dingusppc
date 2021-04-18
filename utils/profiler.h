/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-21 divingkatae and maximum
                      (theweirdo)     spatium

(Contact divingkatae#1017 or powermax#2286 on Discord for more info)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/** Interface definition for the Profiler.
 */

#ifndef PROFILER_H
#define PROFILER_H

#include <cinttypes>
#include <map>
#include <memory>
#include <string>
#include <vector>

enum class ProfileVarFmt { DEC, HEX };

/** Define a special data type for profile variables. */
typedef struct ProfileVar {
    std::string     name;
    ProfileVarFmt   format;
    uint64_t        value;
} ProfileVar;

/** Base class for user-defined profiles. */
class BaseProfile {
public:
    BaseProfile(std::string name) { this->name = name; };

    virtual ~BaseProfile() = default;

    virtual void populate_variables(std::vector<ProfileVar>& vars) = 0;

    virtual void reset(void) = 0; // reset profile counters

private:
    std::string     name;
};

/** Profiler class for managing of user-defined profiles. */
class Profiler {
public:
    Profiler();

    virtual ~Profiler() = default;

    bool register_profile(std::string name, std::unique_ptr<BaseProfile> profile_obj);

    void print_profile(std::string name);

    void reset_profile(std::string name);

private:
    std::map<std::string, std::unique_ptr<BaseProfile>> profiles_map;
};

extern std::unique_ptr<Profiler> gProfilerObj;

#endif /* PROFILER_H */
