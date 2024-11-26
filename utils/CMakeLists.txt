list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(PlatformGlob)

include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
                    )

platform_glob(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(utils OBJECT ${SOURCES})
target_link_libraries(utils PRIVATE)
