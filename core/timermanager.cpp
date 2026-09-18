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

/** Timer management system. */

#include <loguru.hpp>
#include "timermanager.h"

#include <cinttypes>
#include <memory>
#include <mutex>

TimerManager* TimerManager::timer_manager;

void TimerManager::add_absolute_timer(TimerInfo &ti, uint64_t timeout_ns, uint64_t interval, timer_cb cb)
{
    ti.timeout_ns  = timeout_ns;
    ti.interval_ns = interval;
    ti.active      = true;
    ti.cb          = cb;

    // add new timer to the timer queue
    //if (this->timer_queue.remove_by_id(&ti))
    //  LOG_F(ERROR, "timer was in queue!");
    this->timer_queue.push(&ti);

    // notify listeners about changes in the timer queue
    if (!this->cb_active) {
        this->notify_timer_changes();
    }
}

void TimerManager::add_oneshot_timer(TimerInfo &ti, uint64_t timeout, timer_cb cb)
{
    TimerManager::add_absolute_timer(ti, this->get_time_now() + timeout, 0, cb);
}

void TimerManager::add_immediate_timer(TimerInfo &ti, timer_cb cb)
{
    TimerManager::add_absolute_timer(ti, 0, 0, cb);
}

void TimerManager::add_cyclic_timer(TimerInfo &ti, uint64_t interval, uint64_t delay, timer_cb cb)
{
    TimerManager::add_absolute_timer(ti, this->get_time_now() + delay, interval, cb);
}

void TimerManager::add_cyclic_timer(TimerInfo &ti, uint64_t interval, timer_cb cb)
{
    this->add_cyclic_timer(ti, interval, interval, cb);
}

void TimerManager::cancel_timer(TimerInfo &ti)
{
    this->timer_queue.remove_by_id(&ti);
    if (!this->cb_active) {
        this->notify_timer_changes();
    }
}

uint64_t TimerManager::process_timers()
{
    TimerInfo *cur_timer;
    uint64_t time_now = get_time_now();

{ // mtx scope
    std::lock_guard<std::recursive_mutex> lk(this->timer_queue.get_mtx());
    if (this->timer_queue.empty()) {
        return 0ULL;
    }

    // scan for expired timers
    cur_timer = this->timer_queue.top();
} // ] mtx scope
    while (cur_timer->timeout_ns <= time_now) {
        this->timer_queue.remove_by_id(cur_timer);
        uint64_t timeout_ns = cur_timer->timeout_ns;
        timer_cb cb = cur_timer->cb;

        // re-arm cyclic timers
        if (cur_timer->interval_ns) {
            std::lock_guard<std::recursive_mutex> lk(this->timer_queue.get_mtx());
            uint64_t timeout_ns_new = timeout_ns + cur_timer->interval_ns;
            if (timeout_ns_new <= time_now)
                timeout_ns_new = time_now + cur_timer->interval_ns;
            cur_timer->timeout_ns = timeout_ns_new;
            this->timer_queue.push(cur_timer);
        }

        this->cb_active = true;

        // invoke timer callback
        cb(time_now, timeout_ns);

        this->cb_active = false;

        // process next timer
{ // [ mtx scope
        std::lock_guard<std::recursive_mutex> lk(this->timer_queue.get_mtx());
        if (this->timer_queue.empty()) {
            return 0ULL;
        }

        cur_timer = this->timer_queue.top();
} // ] mtx scope
    }

    // return next timer's expiry
    return cur_timer->timeout_ns;
}

void TimerManager::cancel_all_timers()
{
    TimerInfo *cur_timer;
    while (!this->timer_queue.empty()) {
        cur_timer = this->timer_queue.top();
        LOG_F(WARNING, "Canceling timer id:%llu ns:%llu", uint64_t(cur_timer), cur_timer->timeout_ns);
        this->timer_queue.pop();
    }
}
