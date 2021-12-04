include_directories("${PROJECT_SOURCE_DIR}"
                    "${PROJECT_SOURCE_DIR}/thirdparty/loguru/"
                    "${PROJECT_SOURCE_DIR}/thirdparty/SDL2/")

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/common/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/common/adb/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/common/i2c/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/common/pci/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/common/scsi/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/ethernet/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/floppy/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/ioctrl/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/memctrl/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/serial/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/sound/*.cpp"
                  "${CMAKE_CURRENT_SOURCE_DIR}/video/*.cpp"
    )

add_library(devices OBJECT ${SOURCES})
target_link_libraries(devices PRIVATE cubeb)
