#include <stdlib.h>
#include <chrono>
#include "cpu/ppc/ppcemu.h"
#include "cpu/ppc/ppcmmu.h"
#include "devices/mpc106.h"
#include "jittables.h"
#include "nuinterpreter.h"
#include <thirdparty/loguru/loguru.hpp>

uint32_t cs_code[] = {
    0x3863FFFC, 0x7C861671, 0x41820090, 0x70600002, 0x41E2001C, 0xA0030004,
    0x3884FFFE, 0x38630002, 0x5486F0BF, 0x7CA50114, 0x41820070, 0x70C60003,
    0x41820014, 0x7CC903A6, 0x84030004, 0x7CA50114, 0x4200FFF8, 0x5486E13F,
    0x41820050, 0x80030004, 0x7CC903A6, 0x80C30008, 0x7CA50114, 0x80E3000C,
    0x7CA53114, 0x85030010, 0x7CA53914, 0x42400028, 0x80030004, 0x7CA54114,
    0x80C30008, 0x7CA50114, 0x80E3000C, 0x7CA53114, 0x85030010, 0x7CA53914,
    0x4200FFE0, 0x7CA54114, 0x70800002, 0x41E20010, 0xA0030004, 0x38630002,
    0x7CA50114, 0x70800001, 0x41E20010, 0x88030004, 0x5400402E, 0x7CA50114,
    0x7C650194, 0x4E800020
};

/* set up vCPU registers for benchmark code execution */
void setup_regs() {
    ppc_state.pc = 0;
    ppc_state.gpr[3] = 0x1000; // buf
    ppc_state.gpr[4] = 0x8000; // len
    ppc_state.gpr[5] = 0;      // sum
}


int main(int argc, char** argv) {
    int i;

    /* initialize logging */
    loguru::g_preamble_date    = false;
    loguru::g_preamble_time    = false;
    loguru::g_preamble_thread  = false;

    loguru::g_stderr_verbosity = 0;
    loguru::init(argc, argv);

    MPC106* grackle_obj = new MPC106;

    /* we need some RAM */
    if (!grackle_obj->add_ram_region(0, 0x9000)) {
        LOG_F(ERROR, "Could not create RAM region");
        delete(grackle_obj);
        return -1;
    }

    init_jit_tables();

    ppc_cpu_init(grackle_obj, PPC_VER::MPC750);

    /* load executable code into RAM at address 0 */
    for (i = 0; i < sizeof(cs_code); i++) {
        mem_write_dword(i*4, cs_code[i]);
    }

    srand(0xCAFEBABE);

    for (i = 0; i < 32768; i++) {
        mem_write_byte(0x1000+i, rand() % 256);
    }

    setup_regs();

    ppc_exec_until(0xC4);

    LOG_F(INFO, "Checksum: 0x%08X", ppc_state.gpr[3]);

    LOG_F(INFO, "\nRunning with the NuInterpreter");

    setup_regs();

    NuInterpExec(0);

    LOG_F(INFO, "Checksum: 0x%08X", ppc_state.gpr[3]);

    LOG_F(INFO, "\nBenchmarking the old Interpreter:");

    // run the clock once for cache fill etc.
    auto start_time   = std::chrono::steady_clock::now();
    auto end_time     = std::chrono::steady_clock::now();
    auto time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
    LOG_F(INFO, "Time elapsed (dry run): %lld ns", time_elapsed.count());

    for (i = 0; i < 5; i++) {
        setup_regs();

        auto start_time = std::chrono::steady_clock::now();

        ppc_exec_until(0xC4);

        auto end_time = std::chrono::steady_clock::now();

        LOG_F(INFO, "Checksum: 0x%08X", ppc_state.gpr[3]);

        auto time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        LOG_F(INFO, "Time elapsed (run #%d): %lld ns", i, time_elapsed.count());
    }

    LOG_F(INFO, "\nBenchmarking the NuInterpreter:");

    for (i = 0; i < 5; i++) {
        setup_regs();

        auto start_time = std::chrono::steady_clock::now();

        NuInterpExec(0);

        auto end_time = std::chrono::steady_clock::now();

        LOG_F(INFO, "Checksum: 0x%08X", ppc_state.gpr[3]);

        auto time_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time);
        LOG_F(INFO, "Time elapsed (run #%d): %lld ns", i, time_elapsed.count());
    }

    delete(grackle_obj);

    return 0;
}
