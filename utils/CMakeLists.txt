include_directories("${PROJECT_SOURCE_DIR}")

file(GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(utils OBJECT ${SOURCES})
target_link_libraries(utils PRIVATE cubeb)
