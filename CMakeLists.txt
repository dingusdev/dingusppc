cmake_minimum_required(VERSION 3.1)
project(dingusppc)
 
set(CMAKE_CXX_STANDARD 11)
 
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-c -Wall -O3 -s")
 
set(SOURCE_FILES
        macioserial.cpp
        macscsi.cpp
        macswim3.cpp
        mpc106.cpp
        openpic.cpp
        poweropcodes.cpp
        ppcfpopcodes.cpp
        ppcgekkoopcodes.cpp
        ppcmemory.cpp
        ppcopcodes.cpp
        viacuda.cpp
        main.cpp
        davbus.cpp
        )
 
set(HEADER_FILES
        macioserial.h
        macscsi.h
        macswim3.h
        mpc106.h
        openpic.h
        ppcemumain.h
        ppcmemory.h
        viacuda.h
        davbus.h
        )
 
add_executable(dingusppc ${SOURCE_FILES} ${HEADER_FILES})
install (TARGETS dingusppc DESTINATION bin)