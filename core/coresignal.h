/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-23 divingkatae and maximum
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

/** Poor man's signals & slots mechanism implementation. */

// Inspired by http://schneegans.github.io/tutorials/2015/09/20/signal-slot

#ifndef CORE_SIGNAL_H
#define CORE_SIGNAL_H

#include <functional>
#include <map>

template <typename... Args>
class CoreSignal {
public:
    CoreSignal()  = default;
    ~CoreSignal() = default;

    // Connects a std::function type slot to the signal.
    // The return value can be used to disconnect the slot.
    int connect_func(std::function<void(Args...)> const& slot) const {
        _slots.insert(std::make_pair(++_current_id, slot));
        return _current_id;
    }

    // Connects a class method type slot to the signal.
    // The return value can be used to disconnect the slot.
    template <typename T>
    int connect_method(T *inst, void (T::*func)(Args...)) {
        return connect_func([=](Args... args) {
            (inst->*func)(args...);
        });
    }

    // Calls all connected slots.
    void emit(Args... args) {
        if (!_is_enabled) {
            return;
        }
        for (auto const& it : _slots) {
            it.second(args...);
        }
    }

    void disconnect(int id) {
        _slots.erase(id);
    }

    void disconnect_all() {
        _slots.clear();
    }

    void disable() {
        _is_enabled = false;
    }

    void enable() {
        _is_enabled = true;
    }

    bool is_enabled() {
        return _is_enabled;
    }

private:
    mutable std::map<int, std::function<void(Args...)>> _slots;
    mutable unsigned int _current_id { 0 };
    mutable bool _is_enabled { true };
};

#endif // CORE_SIGNAL_H
