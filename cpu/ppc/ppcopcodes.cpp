/*
DingusPPC - The Experimental PowerPC Macintosh emulator
Copyright (C) 2018-24 divingkatae and maximum
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

// General opcodes for the processor - ppcopcodes.cpp

#include <core/timermanager.h>
#include <core/mathutils.h>
#include "ppcemu.h"
#include "ppcdecodemacros.h"
#include "ppcopmacros.h"
#include "ppcopcodes.include"
#include "ppcmmu.h"
#include <cinttypes>
#include <vector>

//Extract the registers desired and the values of the registers.

// Affects CR Field 0 - For integer operations
void ppc_changecrf0(uint32_t set_result) {
    ppc_state.cr =
        (ppc_state.cr & 0x0FFFFFFFU) // clear CR0
        | (
            (set_result == 0) ?
                CRx_bit::CR_EQ
            : (int32_t(set_result) < 0) ?
                CRx_bit::CR_LT
            :
                CRx_bit::CR_GT
        )
        | ((ppc_state.spr[SPR::XER] & XER::SO) >> 3); // copy XER[SO] into CR0[SO].
}

// Affects the XER register's Carry Bit

inline static void ppc_carry(uint32_t a, uint32_t b) {
    if (b < a) {
        ppc_set_xer(XER::CA);
    } else {
        ppc_unset_xer(XER::CA);
    }
}

inline static void ppc_carry_sub(uint32_t a, uint32_t b) {
    if (b >= a) {
        ppc_set_xer(XER::CA);
    } else {
        ppc_unset_xer(XER::CA);
    }
}

// Affects the XER register's SO and OV Bits

inline static void ppc_setsoov(uint32_t a, uint32_t b, uint32_t d) {
    if (int32_t((a ^ b) & (a ^ d)) < 0) {
        ppc_set_xer(XER::SO | XER::OV);
    } else {
        ppc_unset_xer(XER::OV);
    }
}

typedef std::function<void()> CtxSyncCallback;
std::vector<CtxSyncCallback> gCtxSyncCallbacks;

// perform context synchronization by executing registered actions if any
void do_ctx_sync() {
    while (!gCtxSyncCallbacks.empty()) {
        gCtxSyncCallbacks.back()();
        gCtxSyncCallbacks.pop_back();
    }
}

void add_ctx_sync_action(const CtxSyncCallback& cb) {
    gCtxSyncCallbacks.push_back(cb);
}

/** mask generator for rotate and shift instructions (§ 4.2.1.4 PowerpC PEM) */
static inline uint32_t rot_mask(unsigned rot_mb, unsigned rot_me) {
    uint32_t m1 = 0xFFFFFFFFUL >> rot_mb;
    uint32_t m2 = uint32_t(0xFFFFFFFFUL << (31 - rot_me));
    return ((rot_mb <= rot_me) ? m2 & m1 : m1 | m2);
}

static inline void calc_rtcl_value()
{
    uint64_t new_ts = get_virt_time_ns();
    uint64_t rtc_l = new_ts - rtc_timestamp + rtc_lo;
    if (rtc_l >= ONE_BILLION_NS) { // check RTCL overflow
        rtc_hi += (uint32_t)(rtc_l / ONE_BILLION_NS);
        rtc_lo  = rtc_l % ONE_BILLION_NS;
    }
    else {
        rtc_lo = (uint32_t)rtc_l;
    }
    rtc_timestamp = new_ts;
}

static inline uint64_t calc_tbr_value()
{
    uint64_t tbr_inc;
    uint32_t tbr_inc_lo;
    uint64_t diff = get_virt_time_ns() - tbr_wr_timestamp;
    _u32xu64(tbr_freq_ghz, diff, tbr_inc, tbr_inc_lo);
    return (tbr_wr_value + tbr_inc);
}

static inline uint32_t calc_dec_value() {
    uint64_t dec_adj;
    uint32_t dec_adj_lo;
    uint64_t diff = get_virt_time_ns() - dec_wr_timestamp;
    _u32xu64(tbr_freq_ghz, diff, dec_adj, dec_adj_lo);
    return (dec_wr_value - static_cast<uint32_t>(dec_adj));
}

static void update_timebase(uint64_t mask, uint64_t new_val)
{
    uint64_t tbr_value = calc_tbr_value();
    tbr_wr_value = (tbr_value & mask) | new_val;
    tbr_wr_timestamp = get_virt_time_ns();
}


static uint32_t decrementer_timer_id = 0;

static void trigger_decrementer_exception() {
    decrementer_timer_id = 0;
    dec_wr_value = -1;
    dec_wr_timestamp = get_virt_time_ns();
    if (ppc_state.msr & MSR::EE) {
        dec_exception_pending = false;
        //LOG_F(WARNING, "decrementer exception triggered");
        ppc_exception_handler(Except_Type::EXC_DECR, 0);
    }
    else {
        //LOG_F(WARNING, "decrementer exception pending");
        dec_exception_pending = true;
    }
}

static void update_decrementer(uint32_t val) {
    dec_wr_value = val;
    dec_wr_timestamp = get_virt_time_ns();

    dec_exception_pending = false;

    if (is_601)
        return;

    if (decrementer_timer_id) {
        //LOG_F(WARNING, "decrementer cancel timer");
        TimerManager::get_instance()->cancel_timer(decrementer_timer_id);
    }

    uint64_t time_out;
    uint32_t time_out_lo;
    _u32xu64(val, tbr_period_ns, time_out, time_out_lo);
    //LOG_F(WARNING, "decrementer:0x%08X ns:%llu", val, time_out);
    decrementer_timer_id = TimerManager::get_instance()->add_oneshot_timer(
        time_out,
        trigger_decrementer_exception
    );
}
