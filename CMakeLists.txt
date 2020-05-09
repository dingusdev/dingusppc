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
endif()

add_subdirectory("${PROJECT_SOURCE_DIR}/cpu/ppc/")
add_subdirectory("${PROJECT_SOURCE_DIR}/debugger/")
add_subdirectory("${PROJECT_SOURCE_DIR}/devices/")
add_subdirectory("${PROJECT_SOURCE_DIR}/machines/")
add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/loguru/")

set(BUILD_EXAMPLE_PROGRAMS OFF CACHE BOOL "Build libsoundio example programs")
set(BUILD_TESTS OFF CACHE BOOL "Build libsoundio tests")
set(BUILD_DYNAMIC_LIBS ON CACHE BOOL "Build libsoundio dynamic libs")
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build libsoundio static libs")

add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/libsoundio")

include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/devices"
                    "${PROJECT_SOURCE_DIR}/cpu/ppc"
                    "${PROJECT_SOURCE_DIR}/debugger"
					"${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
					"${PROJECT_SOURCE_DIR}/thirdparty/SDL2/"
                    ${LIBSOUNDIO_HEADERS})

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/*.cpp"
                  "${PROJECT_SOURCE_DIR}/*.c"
                  "${PROJECT_SOURCE_DIR}/*.hpp"
                  "${PROJECT_SOURCE_DIR}/*.h")

get_directory_property(LIBSOUNDIO_LIBS DIRECTORY thirdparty/libsoundio DEFINITION LIBSOUNDIO_LIBS)

file(GLOB TEST_SOURCES "${PROJECT_SOURCE_DIR}/cpu/ppc/test/*.cpp")

add_executable(dingusppc ${SOURCES} $<TARGET_OBJECTS:debugger>
                                    $<TARGET_OBJECTS:cpu_ppc>
                                    $<TARGET_OBJECTS:devices>
                                    $<TARGET_OBJECTS:machines>
                                    $<TARGET_OBJECTS:loguru>)

if (WIN32)
target_link_libraries(dingusppc "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2.lib"
                                "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2main.lib"
                                libsoundio_static ${LIBSOUNDIO_LIBS})
else()
target_link_libraries(dingusppc libsoundio_static ${LIBSOUNDIO_LIBS} ${SDL2_LIBRARIES})
endif()


add_executable(testppc ${TEST_SOURCES} $<TARGET_OBJECTS:debugger>
                                       $<TARGET_OBJECTS:cpu_ppc>
                                       $<TARGET_OBJECTS:devices>
                                       $<TARGET_OBJECTS:machines>
                                       $<TARGET_OBJECTS:loguru>)

if (WIN32)
target_link_libraries(testppc "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2.lib"
                              "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/lib/x64/SDL2main.lib"
                              libsoundio_static ${LIBSOUNDIO_LIBS})
else()
target_link_libraries(testppc libsoundio_static ${LIBSOUNDIO_LIBS} ${SDL2_LIBRARIES})
endif()

add_custom_command(
    TARGET testppc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${PROJECT_SOURCE_DIR}/cpu/ppc/test/ppcinttests.csv"
    "${PROJECT_SOURCE_DIR}/cpu/ppc/test/ppcdisasmtest.csv"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

install (TARGETS dingusppc DESTINATION ${EXECUTABLE_OUTPUT_PATH})
