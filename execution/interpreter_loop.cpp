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

#include <chrono>
#include <cinttypes>
#include <ctime>
#include <iostream>
#include <interpreter_loop.h>
#include <loguru.hpp>
#include <cpu/ppc/ppcemu.h>

std::chrono::high_resolution_clock::time_point global;        // global timer
std::chrono::high_resolution_clock::time_point cuda_timer;    // updates every 11 ms
std::chrono::high_resolution_clock::time_point disp_timer;    // updates every 16 ms

using namespace std;

const uint64_t cuda_update    = 11000;
const uint64_t display_update = 16667;

bool cuda_priority = 0;
bool disp_priority = 0;

uint64_t elapsed_times[3] = {0};      // Elapsed time to reach a cycle (for display)
uint64_t routine_bench[3]   = {0};    // Estimated time (in microseconds) to cycle through functions
uint64_t routine_runtime[3] = {0, cuda_update, display_update};    // Time to elapse before execution

enum general_routine_timepoint { OVERALL_UPDATE_TIME, CUDA_UPDATE_TIME, DISPLAY_UPDATE_TIME };

void round_robin_bench() {
    // Benchmark how much time elapses during a minimal CPU block

    std::chrono::high_resolution_clock::time_point dummy = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 4096; i++) {
        dppc_interpreter::ppc_ori(); //execute NOPs as a basic test
    }

    std::chrono::high_resolution_clock::time_point dummy2 = std::chrono::high_resolution_clock::now();
    routine_bench[OVERALL_UPDATE_TIME] =
        std::chrono::duration_cast<std::chrono::microseconds>(dummy2 - dummy).count();
    std::cout << "Initial test: " << routine_bench[OVERALL_UPDATE_TIME] << endl;
}

void interpreter_update_counters() {
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    elapsed_times[OVERALL_UPDATE_TIME] =
        std::chrono::duration_cast<std::chrono::microseconds>(end - global).count();
    elapsed_times[CUDA_UPDATE_TIME] =
        std::chrono::duration_cast<std::chrono::microseconds>(end - cuda_timer).count();
    elapsed_times[DISPLAY_UPDATE_TIME] =
        std::chrono::duration_cast<std::chrono::microseconds>(end - disp_timer).count();

    // Calculate if the threshold for updating a time-critical section has reached or is about to be reached
    if ((elapsed_times[CUDA_UPDATE_TIME] + routine_bench[OVERALL_UPDATE_TIME]) >=
        routine_runtime[CUDA_UPDATE_TIME]) {
        cuda_priority                   = true;
        elapsed_times[CUDA_UPDATE_TIME] = 0;
        cuda_timer                      = end;
    }

    if ((elapsed_times[DISPLAY_UPDATE_TIME] + routine_bench[OVERALL_UPDATE_TIME]) >=
        routine_runtime[DISPLAY_UPDATE_TIME]) {
        disp_priority                      = true;
        elapsed_times[DISPLAY_UPDATE_TIME] = 0;
        disp_timer                         = end;
    }
}

void interpreter_main_loop() {
    // Round robin algorithm goes here
    round_robin_bench();

    global     = std::chrono::high_resolution_clock::now();
    cuda_timer = global;
    disp_timer = global;

    for (;;) {
        if (cuda_priority) {
            LOG_F(9, "Placeholder for Cuda Update function!\n");
            cuda_priority = false;
        }

        if (disp_priority) {
            LOG_F(9, "Placeholder for Display Update function! \n");
            disp_priority = false;
        }

        ppc_exec();

        interpreter_update_counters();
    }
}
