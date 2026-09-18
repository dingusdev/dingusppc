/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-26 The DingusPPC Development Team
          (See CREDITS.MD for more details)

(You may also contact divingkxt or powermax2286 on Discord)

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

#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <atomic>
#include <algorithm>
#include <cinttypes>
#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>

constexpr auto NS_PER_SEC     = 1000000000;
constexpr auto USEC_PER_SEC   = 1000000;
constexpr auto NS_PER_USEC    = 1000;
constexpr auto NS_PER_MSEC    = 1000000;
constexpr auto ONE_BILLION_NS = 1000000000;

#define USECS_TO_NSECS(us) (us) * NS_PER_USEC
#define MSECS_TO_NSECS(ms) (ms) * NS_PER_MSEC

typedef std::function<void(uint64_t timeout_ns, uint64_t time_now)> timer_cb;
typedef std::function<void()> notify_changes_cb;

/** Extend std::priority_queue as suggested here:
    https://stackoverflow.com/a/36711682
    to be able to remove arbitrary elements.
 */
template <typename T, class Container = std::vector<T>, class Compare = std::less<typename Container::value_type>>
class my_priority_queue : public std::priority_queue<T, Container, Compare> {
public:
    bool remove_by_id(T id){
        std::lock_guard<std::recursive_mutex> lk(mtx);
        if (this->empty())
            return false;
        auto el = this->top();
        if (el == id) {
            std::priority_queue<T, Container, Compare>::pop();
            return true;
        }
        auto it = std::find_if(
            this->c.begin(), this->c.end(), [id](const T& el) { return el == id; });
        if (it != this->c.end()) {
            this->c.erase(it);
            std::make_heap(this->c.begin(), this->c.end(), this->comp);
            return true;
        }
        return false;
    }

    void push(T val)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx);
        std::priority_queue<T, Container, Compare>::push(val);
    }

    T pop()
    {
        std::lock_guard<std::recursive_mutex> lk(mtx);
        T val = std::priority_queue<T, Container, Compare>::top();
        std::priority_queue<T, Container, Compare>::pop();
        return val;
    }

    std::recursive_mutex& get_mtx()
    {
        return mtx;
    }

private:
    std::recursive_mutex mtx;
};

typedef struct TimerInfo {
    uint64_t timeout_ns;  // timer expiry
    uint64_t interval_ns; // 0 for one-shot timers
    bool     active = false; // set to true when added to the queue
    timer_cb cb;          // timer callback
} TimerInfo;

// Custom comparator for sorting our timer queue in ascending order
class MyGtComparator {
public:
    bool operator()(const TimerInfo *l, const TimerInfo *r) const {
        return l->timeout_ns > r->timeout_ns ||
            (l->timeout_ns == r->timeout_ns && l > r);
    }
};

class TimerManager {
public:
    static TimerManager* get_instance() {
        if (!timer_manager) {
            timer_manager = new TimerManager();
        }
        return timer_manager;
    }

    // callback for retrieving current time
    void set_time_now_cb(const std::function<uint64_t()> &cb) {
        this->get_time_now = cb;
    }

    // callback for acknowledging time changes
    void set_notify_changes_cb(const notify_changes_cb &cb) {
        this->notify_timer_changes = cb;
    }

    // return current virtual time in nanoseconds
    uint64_t current_time_ns() const { return get_time_now(); }

    // creating and cancelling timers
    void add_absolute_timer(TimerInfo &ti, uint64_t timeout_ns, uint64_t interval, timer_cb cb);
    void add_oneshot_timer(TimerInfo &ti, uint64_t timeout, timer_cb cb);
    void add_immediate_timer(TimerInfo &ti, timer_cb cb);
    void add_cyclic_timer(TimerInfo &ti, uint64_t interval, timer_cb cb);
    void add_cyclic_timer(TimerInfo &ti, uint64_t interval, uint64_t delay, timer_cb cb);
    void cancel_timer(TimerInfo &ti);
    void cancel_all_timers();

    uint64_t process_timers();

private:
    static TimerManager* timer_manager;
    TimerManager(){} // private constructor to implement a singleton

    // timer queue
    my_priority_queue<TimerInfo*, std::vector<TimerInfo*>, MyGtComparator> timer_queue;

    std::function<uint64_t()>   get_time_now;
    std::function<void()>       notify_timer_changes;

    // FIXME: Do we need this? It gets written in main thread and read in audio thread.
    bool cb_active = false; // true if a timer callback is executing
};

#endif // TIMER_MANAGER_H
