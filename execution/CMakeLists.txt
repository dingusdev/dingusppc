include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/")

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(execution OBJECT ${SOURCES})
target_link_libraries(execution PRIVATE cubeb)
