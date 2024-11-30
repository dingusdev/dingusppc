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

#ifndef DEBUGGER_H_
#define DEBUGGER_H_

#include <memory>

class DppcDebugger {
public:
    static DppcDebugger* get_instance() {
        if (!debugger_obj) {
            debugger_obj = std::unique_ptr<DppcDebugger>(new DppcDebugger());
        }
        return debugger_obj.get();
    };

    void enter_debugger();

private:
    inline static std::unique_ptr<DppcDebugger> debugger_obj{nullptr};
    explicit DppcDebugger(); // private constructor to implement a singleton
};

#endif    // DEBUGGER_H_
