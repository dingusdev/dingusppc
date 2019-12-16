cmake_minimum_required(VERSION 3.1)
project(dingusppc)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-c -Wall -O3 -s")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/devices"
                    "${PROJECT_SOURCE_DIR}/debugger")

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/*.cpp" "${PROJECT_SOURCE_DIR}/devices/*.cpp"
                  "${PROJECT_SOURCE_DIR}/debugger/*.cpp")

add_executable(dingusppc ${SOURCES})
install (TARGETS dingusppc DESTINATION ${EXECUTABLE_OUTPUT_PATH})
