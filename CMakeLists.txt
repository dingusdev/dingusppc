cmake_minimum_required(VERSION 3.1)
project(dingusppc)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

if (NOT WIN32)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
else()
include_directories("${PROJECT_SOURCE_DIR}/thirdparty/SDL2/include/")
if (UNIX AND NOT APPLE)
    find_package (Threads)
endif()
endif()

option(DPPC_BUILD_PPC_TESTS  "Build PowerPC tests" OFF)
option(DPPC_BUILD_BENCHMARKS "Build benchmarking programs" OFF)

option(ENABLE_68K_DEBUGGER   "Enable 68k debugging" OFF)

if (ENABLE_68K_DEBUGGER)
    # Build capstone as static library.
    set(CAPSTONE_BUILD_STATIC ON CACHE BOOL "")

    # Turn off anything unnecessary.
    set(CAPSTONE_BUILD_SHARED OFF CACHE BOOL "")
    set(CAPSTONE_BUILD_TESTS OFF CACHE BOOL "")
    set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "")
    set(CAPSTONE_BUILD_DIET OFF CACHE BOOL "")
    set(CAPSTONE_OSXKERNEL_SUPPORT OFF CACHE BOOL "")

    # Disable unused Capstone architectures.
    set(CAPSTONE_ARM_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_ARM64_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_MIPS_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_PPC_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_SPARC_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_SYSZ_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_XCORE_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_X86_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_TMS320C64X_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_M680X_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_EVM_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_MOS65XX_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_WASM_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_BPF_SUPPORT OFF CACHE BOOL "")
    set(CAPSTONE_RISCV_SUPPORT OFF CACHE BOOL "")

    ADD_DEFINITIONS(-DENABLE_68K_DEBUGGER)

    add_subdirectory(thirdparty/capstone EXCLUDE_FROM_ALL)
endif()

add_subdirectory("${PROJECT_SOURCE_DIR}/cpu/ppc/")
add_subdirectory("${PROJECT_SOURCE_DIR}/debugger/")
add_subdirectory("${PROJECT_SOURCE_DIR}/devices/")
add_subdirectory("${PROJECT_SOURCE_DIR}/execution")
add_subdirectory("${PROJECT_SOURCE_DIR}/machines/")
add_subdirectory("${PROJECT_SOURCE_DIR}/utils/")
add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/loguru/")

#set(BUILD_EXAMPLE_PROGRAMS OFF CACHE BOOL "Build libsoundio example programs")
#set(BUILD_TESTS OFF CACHE BOOL "Build libsoundio tests")
#set(BUILD_DYNAMIC_LIBS ON CACHE BOOL "Build libsoundio dynamic libs")
#set(BUILD_STATIC_LIBS ON CACHE BOOL "Build libsoundio static libs")

#add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/libsoundio")

set(BUILD_TESTS OFF CACHE BOOL "Build Cubeb tests")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
set(BUILD_TOOLS OFF CACHE BOOL "Build Cubeb tools")

add_subdirectory(thirdparty/cubeb EXCLUDE_FROM_ALL)

set(CLI11_ROOT ${PROJECT_SOURCE_DIR}/thirdparty/CLI11)

include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/devices"
                    "${PROJECT_SOURCE_DIR}/cpu/ppc"
                    "${PROJECT_SOURCE_DIR}/debugger"
                    "${PROJECT_SOURCE_DIR}/utils"
					"${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
#                    ${SDL2_INCLUDE_DIRS})
					"${PROJECT_SOURCE_DIR}/thirdparty/SDL2/")
#                    "${PROJECT_SOURCE_DIR}/thirdparty/cubeb/include")
#                    ${LIBSOUNDIO_HEADERS})

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/*.cpp"
                  "${PROJECT_SOURCE_DIR}/*.c"
                  "${PROJECT_SOURCE_DIR}/*.hpp"
                  "${PROJECT_SOURCE_DIR}/*.h")

#get_directory_property(LIBSOUNDIO_LIBS DIRECTORY thirdparty/libsoundio DEFINITION LIBSOUNDIO_LIBS)

file(GLOB TEST_SOURCES "${PROJECT_SOURCE_DIR}/cpu/ppc/test/*.cpp")

add_executable(dingusppc ${SOURCES} $<TARGET_OBJECTS:debugger>
                                    $<TARGET_OBJECTS:cpu_ppc>
                                    $<TARGET_OBJECTS:devices>
                                    $<TARGET_OBJECTS:execution>
                                    $<TARGET_OBJECTS:machines>
                                    $<TARGET_OBJECTS:utils>
                                    $<TARGET_OBJECTS:loguru>)

if (WIN32)
target_link_libraries(dingusppc "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2.lib"
                                "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2main.lib"
                                cubeb)
else()
#target_link_libraries(dingusppc libsoundio_static ${LIBSOUNDIO_LIBS} ${SDL2_LIBRARIES})
target_link_libraries(dingusppc cubeb ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(dingusppc PRIVATE ${CLI11_ROOT})
endif()

if (ENABLE_68K_DEBUGGER)
    target_link_libraries(dingusppc capstone-static)
endif()

if (DPPC_BUILD_PPC_TESTS)
    add_executable(testppc ${TEST_SOURCES} $<TARGET_OBJECTS:debugger>
                                           $<TARGET_OBJECTS:cpu_ppc>
                                           $<TARGET_OBJECTS:devices>
                                           $<TARGET_OBJECTS:execution>
                                           $<TARGET_OBJECTS:machines>
                                           $<TARGET_OBJECTS:utils>
                                           $<TARGET_OBJECTS:loguru>)

    if (WIN32)
        target_link_libraries(testppc
                    "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2.lib"
                    "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2main.lib"
                    cubeb)
    else()
        #target_link_libraries(testppc libsoundio_static ${LIBSOUNDIO_LIBS} ${SDL2_LIBRARIES})
        target_link_libraries(testppc cubeb ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS}
                    ${CMAKE_THREAD_LIBS_INIT})
    endif()

    if (ENABLE_68K_DEBUGGER)
        target_link_libraries(testppc capstone-static)
    endif()
endif()

if (DPPC_BUILD_BENCHMARKS)
    file(GLOB BENCH_SOURCES "${PROJECT_SOURCE_DIR}/benchmark/*.cpp")
    add_executable(bench1 ${BENCH_SOURCES} $<TARGET_OBJECTS:debugger>
                                           $<TARGET_OBJECTS:cpu_ppc>
                                           $<TARGET_OBJECTS:devices>
                                           $<TARGET_OBJECTS:execution>
                                           $<TARGET_OBJECTS:machines>
                                           $<TARGET_OBJECTS:utils>
                                           $<TARGET_OBJECTS:loguru>)

    target_link_libraries(bench1 cubeb ${SDL2_LIBRARIES} ${CMAKE_DL_LIBS}
            ${CMAKE_THREAD_LIBS_INIT})

    if (ENABLE_68K_DEBUGGER)
        target_link_libraries(bench1 capstone-static)
    endif()
endif()

if (DPPC_BUILD_PPC_TESTS)
    add_custom_command(
        TARGET testppc POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        "${PROJECT_SOURCE_DIR}/cpu/ppc/test/ppcinttests.csv"
        "${PROJECT_SOURCE_DIR}/cpu/ppc/test/ppcdisasmtest.csv"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)
endif()

install (TARGETS dingusppc DESTINATION ${PROJECT_SOURCE_DIR}/build)
