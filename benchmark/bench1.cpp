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

#include <stdlib.h>
#include <chrono>
#include "cpu/ppc/ppcemu.h"
#include "cpu/ppc/ppcmmu.h"
#include "devices/memctrl/mpc106.h"
#include <thirdparty/loguru/loguru.hpp>
#include <debugger/debugger.h>

#if defined(PPC_BENCHMARKS)
void ppc_exception_handler(Except_Type exception_type, uint32_t srr1_bits) {
    power_on = false;
    power_off_reason = po_benchmark_exception;
}
#endif

uint32_t cs_code[] = {
    0x3863FFFC, 0x7C861671, 0x41820090, 0x70600002, 0x41E2001C, 0xA0030004,
    0x3884FFFE, 0x38630002, 0x5486F0BF, 0x7CA50114, 0x41820070, 0x70C60003,
    0x41820014, 0x7CC903A6, 0x84030004, 0x7CA50114, 0x4200FFF8, 0x5486E13F,
    0x41820050, 0x80030004, 0x7CC903A6, 0x80C30008, 0x7CA50114, 0x80E3000C,
    0x7CA53114, 0x85030010, 0x7CA53914, 0x42400028, 0x80030004, 0x7CA54114,
    0x80C30008, 0x7CA50114, 0x80E3000C, 0x7CA53114, 0x85030010, 0x7CA53914,
    0x4200FFE0, 0x7CA54114, 0x70800002, 0x41E20010, 0xA0030004, 0x38630002,
    0x7CA50114, 0x70800001, 0x41E20010, 0x88030004, 0x5400402E, 0x7CA50114,
    0x7C650194, /* 0x4E800020 */ 0x00005AF0
};

constexpr uint32_t test_size = 0x8000; // 0x7FFFFFFC is the max
constexpr uint32_t test_samples = 200;
constexpr uint32_t test_iterations = 5;

int main(int argc, char** argv) {
    int i, j;

    /* initialize logging */
    loguru::g_preamble_date    = false;
    loguru::g_preamble_time    = false;
    loguru::g_preamble_thread  = false;

    loguru::g_stderr_verbosity = 0;
    loguru::init(argc, argv);

    MPC106* grackle_obj = new MPC106;

    /* we need some RAM */
    if (!grackle_obj->add_ram_region(0, 0x1000 + test_size)) {
        LOG_F(ERROR, "Could not create RAM region");
        delete(grackle_obj);
        return -1;
    }

    constexpr uint64_t tbr_freq = 16705000;

    ppc_cpu_init(grackle_obj, PPC_VER::MPC750, false, tbr_freq);

    /* load executable code into RAM at address 0 */
    for (i = 0; i < sizeof(cs_code) / sizeof(cs_code[0]); i++) {
        mmu_write_vmem<uint32_t>(0, i * 4, cs_code[i]);
    }

    srand(0xCAFEBABE);

    LOG_F(INFO, "Test size: 0x%X", test_size);
    LOG_F(INFO, "First few bytes:");
    bool did_lf = false;
    for (i = 0; i < test_size; i++) {
        uint8_t val = rand() % 256;
        mmu_write_vmem<uint8_t>(0, 0x1000+i, val);
        if (i < 64) {
            printf("%02x", val);
            did_lf = false;
            if (i % 32 == 31) {
                printf("\n");
                did_lf = true;
            }
        }
    }
    if (!did_lf)
        printf("\n");

#if 0
    /* prepare benchmark code execution */
    ppc_state.pc = 0;
    ppc_state.gpr[3] = 0x1000;    // buf
    ppc_state.gpr[4] = test_size; // len
    ppc_state.gpr[5] = 0;         // sum
    enter_debugger();
#endif

    ppc_state.pc = 0;
    ppc_state.gpr[3] = 0x1000;    // buf
    ppc_state.gpr[4] = test_size; // len
    ppc_state.gpr[5] = 0;         // sum
    power_on = true;
    ppc_exec_until(0xC4);

#if SUPPORTS_PPC_LITTLE_ENDIAN_MODE
    #warning PPC endian mode enabled
    LOG_F(INFO, "PPC endian mode enabled");
#else
    #warning PPC endian mode disabled
    LOG_F(INFO, "PPC endian mode disabled");
#endif

#if SUPPORTS_MEMORY_CTRL_ENDIAN_MODE
    #warning Memory endian mode enabled
    LOG_F(INFO, "Memory endian mode enabled");
#else
    #warning Memory endian mode disabled
    LOG_F(INFO, "Memory endian mode disabled");
#endif

#ifdef LOG_INSTRUCTIONS
    #warning Log instructions enabled
    LOG_F(INFO, "Log instructions enabled");
#else
    #warning Log instructions disabled
    LOG_F(INFO, "Log instructions disabled");
#endif

    LOG_F(INFO, "Checksum: 0x%08X", ppc_state.gpr[3]);
    uint32_t checksum = ppc_state.gpr[3];

    // run the clock once for cache fill etc.
    uint64_t overhead = -1;
    for (j = 0; j < test_samples; j++) {
        auto start_time   = std::chrono::steady_clock::now();
        auto end_time     = std::chrono::steady_clock::now();
        auto time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        if (time_elapsed.count() < overhead)
            overhead = time_elapsed.count();
    }
    LOG_F(INFO, "Overhead Time: %lld ns", overhead);

    for (int theproc = 0; theproc < 2; theproc++) {
        LOG_F(INFO, "Doing %s", theproc ? "ppc_exec_until" : "ppc_exec");
        for (i = 0; i < test_iterations; i++) {
            uint64_t best_sample = -1;
            for (j = 0; j < test_samples; j++) {
                ppc_state.pc = 0;
                ppc_state.gpr[3] = 0x1000;    // buf
                ppc_state.gpr[4] = test_size; // len
                ppc_state.gpr[5] = 0;         // sum
                power_on = true;

                auto start_time   = std::chrono::steady_clock::now();
                    (theproc) ? ppc_exec_until(0xC4) : ppc_exec();
                auto end_time     = std::chrono::steady_clock::now();
                auto time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
                if (time_elapsed.count() < best_sample)
                    best_sample = time_elapsed.count();
            }
            if (ppc_state.gpr[3] != checksum)
                LOG_F(INFO, "Checksum: 0x%08X", ppc_state.gpr[3]);
            best_sample -= overhead;
            LOG_F(INFO, "(%d) %lld ns, %.4lf MiB/s", i+1, best_sample, 1E9 * test_size / (best_sample * 1024 * 1024));
        }
    }

    delete(grackle_obj);

    return 0;
}
