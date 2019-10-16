cmake_minimum_required(VERSION 3.1)
project(dingusppc)
 
set(CMAKE_CXX_STANDARD 11)
 
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-c -Wall -O3 -s")
 
set(SOURCE_FILES
        devices/heathrow.cpp
        devices/memctrlbase.cpp
        devices/mpc106.cpp
        devices/viacuda.cpp
        poweropcodes.cpp
        ppcfpopcodes.cpp
        ppcgekkoopcodes.cpp
        ppcmemory.cpp
        ppcopcodes.cpp
        main.cpp
        debugger.cpp
        )
 
set(HEADER_FILES
        devices/heathrow.cpp
        devices/machineid.h
        devices/macio.h
        devices/memctrlbase.h
        devices/mmiodevice.h
        devices/mpc106.h
        devices/pcidevice.h
        devices/pcihost.h
        devices/viacuda.h
        ppcemumain.h
        ppcmemory.h
        debugger.h
        )
 
add_executable(dingusppc ${SOURCE_FILES} ${HEADER_FILES})
install (TARGETS dingusppc DESTINATION bin)
