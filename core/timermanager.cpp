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

/** Timer management system. */

#include <loguru.hpp>
#include "timermanager.h"

#include <cinttypes>

TimerManager* TimerManager::timer_manager;

uint32_t TimerManager::add_oneshot_timer(uint64_t timeout, timer_cb cb)
{
    if (timeout <= MIN_TIMEOUT_NS) {
        LOG_F(WARNING, "One-shot timer too short, timeout=%llu ns", (long long unsigned)timeout);
    }

    TimerInfo* ti = new TimerInfo;

    ti->id          = ++this->id;
    ti->timeout_ns  = this->get_time_now() + timeout;
    ti->interval_ns = 0;
    ti->cb          = cb;

    std::shared_ptr<TimerInfo> timer_desc(ti);

    // add new timer to the timer queue
    this->timer_queue.push(timer_desc);

    // notify listeners about changes in the timer queue
    if (!this->cb_active) {
        this->notify_timer_changes();
    }

    return ti->id;
}

uint32_t TimerManager::add_immediate_timer(timer_cb cb) {
    TimerInfo* ti = new TimerInfo;

    ti->id          = ++this->id;
    ti->timeout_ns  = this->get_time_now() + 10;
    ti->interval_ns = 0;
    ti->cb          = cb;

    std::shared_ptr<TimerInfo> timer_desc(ti);

    // add new timer to the timer queue
    this->timer_queue.push(timer_desc);

    // notify listeners about changes in the timer queue
    if (!this->cb_active) {
        this->notify_timer_changes();
    }

    return ti->id;
}

uint32_t TimerManager::add_cyclic_timer(uint64_t interval, uint64_t delay, timer_cb cb)
{
    if (!interval || interval <= MIN_TIMEOUT_NS) {
        LOG_F(WARNING, "Cyclic timer interval too short, timeout=%llu ns",
            (long long unsigned)interval);
    }

    TimerInfo* ti = new TimerInfo;

    ti->id          = ++this->id;
    ti->timeout_ns  = this->get_time_now() + delay;
    ti->interval_ns = interval;
    ti->cb          = cb;

    std::shared_ptr<TimerInfo> timer_desc(ti);

    // add new timer to the timer queue
    this->timer_queue.push(timer_desc);

    // notify listeners about changes in the timer queue
    if (!this->cb_active) {
        this->notify_timer_changes();
    }

    return ti->id;
}

uint32_t TimerManager::add_cyclic_timer(uint64_t interval, timer_cb cb) {
    return this->add_cyclic_timer(interval, interval, cb);
}

void TimerManager::cancel_timer(uint32_t id)
{
    TimerInfo* cur_timer = this->timer_queue.top().get();
    if (cur_timer->id == id) {
        this->timer_queue.pop();
    } else {
        this->timer_queue.remove_by_id(id);
    }
    if (!this->cb_active) {
        this->notify_timer_changes();
    }
}

uint64_t TimerManager::process_timers(uint64_t time_now)
{
    TimerInfo* cur_timer;

    if (this->timer_queue.empty()) {
        return 0ULL;
    }

    // scan for expired timers
    cur_timer = this->timer_queue.top().get();
    while (cur_timer->timeout_ns <= time_now ||
           cur_timer->timeout_ns <= (time_now + MIN_TIMEOUT_NS)) {
        timer_cb cb = cur_timer->cb;

        // re-arm cyclic timers
        if (cur_timer->interval_ns) {
            auto new_timer = this->timer_queue.top();
            new_timer->timeout_ns = time_now + cur_timer->interval_ns;
            this->timer_queue.pop();
            this->timer_queue.push(new_timer);
        } else {
            // remove one-shot timers from queue
            this->timer_queue.pop();
        }

        this->cb_active = true;

        // invoke timer callback
        cb();

        this->cb_active = false;

        // process next timer
        if (this->timer_queue.empty()) {
            return 0ULL;
        }

        cur_timer = this->timer_queue.top().get();
    }

    // return time slice in nanoseconds until next timer's expiry
    return cur_timer->timeout_ns - time_now;
}
