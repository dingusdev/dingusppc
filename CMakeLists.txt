cmake_minimum_required(VERSION 3.1)
project(dingusppc)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

add_subdirectory("${PROJECT_SOURCE_DIR}/cpu/ppc/")
add_subdirectory("${PROJECT_SOURCE_DIR}/devices/")
add_subdirectory("${PROJECT_SOURCE_DIR}/thirdparty/")

include_directories("${PROJECT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/devices"
                    "${PROJECT_SOURCE_DIR}/cpu/ppc"
                    "${PROJECT_SOURCE_DIR}/debugger"
					"${PROJECT_SOURCE_DIR}/thirdparty")

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/*.cpp"
                  "${PROJECT_SOURCE_DIR}/debugger/*.cpp")

file(GLOB TEST_SOURCES "${PROJECT_SOURCE_DIR}/cpu/ppc/test/*.cpp")

add_executable(dingusppc ${SOURCES} $<TARGET_OBJECTS:cpu_ppc> $<TARGET_OBJECTS:devices>
                                    $<TARGET_OBJECTS:loguru>)

add_executable(testppc ${TEST_SOURCES} $<TARGET_OBJECTS:cpu_ppc> $<TARGET_OBJECTS:devices>
                                       $<TARGET_OBJECTS:loguru>)

add_custom_command(
    TARGET testppc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    "${PROJECT_SOURCE_DIR}/cpu/ppc/test/ppcinttests.csv"
    "${PROJECT_SOURCE_DIR}/cpu/ppc/test/ppcdisasmtest.csv"
    $<TARGET_FILE_DIR:${PROJECT_NAME}>)

install (TARGETS dingusppc DESTINATION ${EXECUTABLE_OUTPUT_PATH})
