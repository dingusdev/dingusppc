include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
                    )

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(core OBJECT ${SOURCES})
target_link_libraries(core PRIVATE SDL2::SDL2)
