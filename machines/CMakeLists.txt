set(CMAKE_CXX_STANDARD 11)

include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/")

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(machines OBJECT ${SOURCES})
target_link_libraries(machines PRIVATE cubeb)
