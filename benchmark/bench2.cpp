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

// Replay floating-point instructions and input registers captured while BeOS
// PR1 GLTeapot renders. This measures the real opcode dispatcher and handlers,
// including FPSCR maintenance, without guest boot, display, or timer overhead.
#include <cpu/ppc/ppcemu.h>
#include <thirdparty/loguru/loguru.hpp>

#include <algorithm>
#include <cfenv>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <vector>

#include "bench2_data.h"

#if defined(PPC_BENCHMARKS)
void ppc_exception_handler(Except_Type exception_type, uint32_t srr1_bits) {
    power_off(po_benchmark_exception);
}
#endif

static void replay_sample(const FpuSample& sample) {
    if ((ppc_state.fpscr ^ sample.fpscr) & FPSCR::RN_MASK)
        set_host_rounding_mode(sample.fpscr & FPSCR::RN_MASK);
    ppc_state.fpscr = sample.fpscr;
    ppc_state.cr = sample.cr;
    ppc_state.fpr[(sample.opcode >> 21) & 31].int64_r = sample.d;
    ppc_state.fpr[(sample.opcode >> 16) & 31].int64_r = sample.a;
    ppc_state.fpr[(sample.opcode >> 11) & 31].int64_r = sample.b;
    ppc_state.fpr[(sample.opcode >> 6) & 31].int64_r = sample.c;
    ppc_main_opcode(ppc_opcode_grabber, sample.opcode);
}

static uint64_t check_replay() {
    uint64_t hash = 1469598103934665603ULL;
    auto mix = [&hash](uint64_t value) { hash = (hash ^ value) * 1099511628211ULL; };
    std::feclearexcept(FE_ALL_EXCEPT);
    for (const auto& sample : fpu_samples) {
        replay_sample(sample);
        mix(ppc_state.fpr[(sample.opcode >> 21) & 31].int64_r);
        mix(ppc_state.fpscr);
        mix(ppc_state.cr);
    }
    return hash;
}

int main(int argc, char** argv) {
    if (argc > 2 || (argc == 2 && std::strcmp(argv[1], "--check"))) {
        fprintf(stderr, "Usage: %s [--check]\n", argv[0]);
        return 2;
    }
    loguru::g_stderr_verbosity = loguru::Verbosity_ERROR;
    is_601 = false;
    initialize_ppc_opcode_table();
    ppc_msr_did_change(ppc_state.msr, MSR::ME | MSR::IP | MSR::FP, false);
    set_host_rounding_mode(0);
    power_on = true;

    const uint64_t expected = check_replay();
    constexpr size_t sample_count = sizeof(fpu_samples) / sizeof(fpu_samples[0]);
    printf("GLTeapot FPU replay: %zu samples, checksum %016" PRIx64 "\n", sample_count, expected);
    if (!power_on)
        return 1;
    if (argc == 2 && !std::strcmp(argv[1], "--check"))
        return 0;

    constexpr unsigned passes = 10000;
    constexpr unsigned trials = 9;
    // Warm the dispatcher, handlers and captured input data.
    for (unsigned pass = 0; pass < 100; pass++)
        for (const auto& sample : fpu_samples)
            replay_sample(sample);

    std::vector<double> times;
    for (unsigned trial = 0; trial < trials; trial++) {
        std::feclearexcept(FE_ALL_EXCEPT);
        auto start = std::chrono::steady_clock::now();
        for (unsigned pass = 0; pass < passes; pass++)
            for (const auto& sample : fpu_samples)
                replay_sample(sample);
        auto end = std::chrono::steady_clock::now();
        if (!power_on || check_replay() != expected) {
            fprintf(stderr, "FPU replay changed its result during timing\n");
            return 1;
        }
        double ns = std::chrono::duration<double, std::nano>(end - start).count() /
                    (uint64_t(passes) * sample_count);
        times.push_back(ns);
        printf("Trial %u: %.3f ns/FPU instruction, %.3f million FPU instructions/s\n",
               trial + 1, ns, 1000.0 / ns);
    }
    std::sort(times.begin(), times.end());
    double median = times[times.size() / 2];
    printf("Median: %.3f ns/FPU instruction, %.3f million FPU instructions/s\n",
           median, 1000.0 / median);
    return 0;
}
