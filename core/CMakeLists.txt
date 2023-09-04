include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
                    )

platform_glob(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(core OBJECT ${SOURCES})
if (EMSCRIPTEN)
    target_link_libraries(core PRIVATE)
else()
    target_link_libraries(core PRIVATE SDL2::SDL2)
endif()
