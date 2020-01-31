cmake_minimum_required(VERSION 3.1)
project(dingusppc)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/devices"
                    "${PROJECT_SOURCE_DIR}/debugger"
                    "${PROJECT_SOURCE_DIR}/cpu/ppc")

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/*.cpp" "${PROJECT_SOURCE_DIR}/devices/*.cpp"
                  "${PROJECT_SOURCE_DIR}/debugger/*.cpp"
                  "${PROJECT_SOURCE_DIR}/cpu/ppc/*.cpp")

file(GLOB TEST_SOURCES "${PROJECT_SOURCE_DIR}/cpu/ppc/*.cpp"
                       "${PROJECT_SOURCE_DIR}/devices/*.cpp"
                       "${PROJECT_SOURCE_DIR}/cpu/ppc/test/*.cpp")

add_executable(dingusppc ${SOURCES})

add_executable(testppc ${TEST_SOURCES})

install (TARGETS dingusppc DESTINATION ${EXECUTABLE_OUTPUT_PATH})
