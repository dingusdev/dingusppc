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

#define NS_PER_SEC      1000000000
#define USEC_PER_SEC    1000000
#define NS_PER_USEC     1000
#define NS_PER_MSEC     1000000
#define ONE_BILLION_NS  1000000000

#define USECS_TO_NSECS(us) (us) * NS_PER_USEC
#define MSECS_TO_NSECS(ms) (ms) * NS_PER_MSEC

typedef std::function<void()> timer_cb;

/** Extend std::priority_queue as suggested here:
    https://stackoverflow.com/a/36711682
    to be able to remove arbitrary elements.
 */
template <typename T, class Container = std::vector<T>, class Compare = std::less<typename Container::value_type>>
class my_priority_queue : public std::priority_queue<T, Container, Compare> {
public:
    bool remove_by_id(const uint32_t id){
        std::lock_guard<std::recursive_mutex> lk(mtx);
        auto el = this->top();
        if (el->id == id) {
            std::priority_queue<T, Container, Compare>::pop();
            return true;
        }
        auto it = std::find_if(
            this->c.begin(), this->c.end(), [id](const T& el) { return el->id == id; });
        if (it != this->c.end()) {
            this->c.erase(it);
            std::make_heap(this->c.begin(), this->c.end(), this->comp);
            return true;
        }
        return false;
    };

    void push(T val)
    {
        std::lock_guard<std::recursive_mutex> lk(mtx);
        std::priority_queue<T, Container, Compare>::push(val);
    };

    T pop()
    {
        std::lock_guard<std::recursive_mutex> lk(mtx);
        T val = std::priority_queue<T, Container, Compare>::top();
        std::priority_queue<T, Container, Compare>::pop();
        return val;
    };

    std::recursive_mutex& get_mtx()
    {
        return mtx;
    }

private:
    std::recursive_mutex mtx;
};

typedef struct TimerInfo {
    uint32_t id;
    uint64_t timeout_ns;  // timer expiry
    uint64_t interval_ns; // 0 for one-shot timers
    timer_cb cb;          // timer callback
} TimerInfo;

// Custom comparator for sorting our timer queue in ascending order
class MyGtComparator {
public:
    bool operator()(const std::shared_ptr<TimerInfo>& l, const std::shared_ptr<TimerInfo>& r) {
        return l.get()->timeout_ns > r.get()->timeout_ns;
    }
};

class TimerManager {
public:
    static TimerManager* get_instance() {
        if (!timer_manager) {
            timer_manager = new TimerManager();
        }
        return timer_manager;
    };

    // callback for retrieving current time
    void set_time_now_cb(const std::function<uint64_t()> &cb) {
        this->get_time_now = cb;
    };

    // callback for acknowledging time changes
    void set_notify_changes_cb(const timer_cb &cb) {
        this->notify_timer_changes = cb;
    };

    // return current virtual time in nanoseconds
    uint64_t current_time_ns() { return get_time_now(); };

    // creating and cancelling timers
    uint32_t add_oneshot_timer(uint64_t timeout, timer_cb cb);
    uint32_t add_immediate_timer(timer_cb cb);
    uint32_t add_cyclic_timer(uint64_t interval, timer_cb cb);
    uint32_t add_cyclic_timer(uint64_t interval, uint64_t delay, timer_cb cb);
    void cancel_timer(uint32_t id);

    uint64_t process_timers();

private:
    static TimerManager* timer_manager;
    TimerManager(){}; // private constructor to implement a singleton

    // timer queue
    my_priority_queue<std::shared_ptr<TimerInfo>, std::vector<std::shared_ptr<TimerInfo>>, MyGtComparator> timer_queue;

    std::function<uint64_t()>   get_time_now;
    std::function<void()>       notify_timer_changes;

    std::atomic<uint32_t> id{0};
    bool        cb_active = false; // true if a timer callback is executing // FIXME: Do we need this? It gets written in main thread and read in audio thread.
};

#endif // TIMER_MANAGER_H
