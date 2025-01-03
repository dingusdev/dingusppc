list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(PlatformGlob)

include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
                   )

platform_glob(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/adb/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/clockgen/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/i2c/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/ata/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/nubus/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/pci/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/scsi/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/ethernet/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/floppy/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/ioctrl/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/memctrl/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/serial/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/sound/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/video/*.cpp"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/adb/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/firewire/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/i2c/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/ata/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/pci/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/scsi/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/common/usb/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/ethernet/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/floppy/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/ioctrl/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/memctrl/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/serial/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/sound/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/storage/*.h"
                      "${CMAKE_CURRENT_SOURCE_DIR}/video/*.h"
    )

add_library(devices OBJECT ${SOURCES})
if (EMSCRIPTEN)
    target_link_libraries(devices PRIVATE)
else()
    target_link_libraries(devices PRIVATE cubeb SDL2::SDL2)
endif()
