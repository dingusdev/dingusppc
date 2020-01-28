# DingusPPC by divingkatae

Be warned the program is highly unfinished and could use a lot of testing. Any feedback is welcome.

## PHILOSOPHY OF USE

Sheepshaver, while technically impressive, is becoming harder to compile and run. While many other PowerPC emus exist, none of them currently attempt emulation of PPC Macs (except for QEMU).

This program aims to not only improve upon what Sheepshaver has done, but also to provide a better debugging environment. This currently is designed to work best with PowerPC Old World ROMs,
including those of the PowerMac G3 Beige.

## HOW TO USE

This program currently uses the command prompt to work.

There are a few command line arguments one must enter when starting the program.

-realtime

Run the emulator in runtime.

-debugger

Enter the interactive debugger.

## HOW TO COMPILE

You'll need to install development tools first.

At least, a C++ compiler and [CMake](https://cmake.org) are required.

For example, to build the project in a Unix-like environment, you'll need to run
the following commands in the OS terminal:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```
You may specify another build type using the variable CMAKE_BUILD_TYPE.

Due to the incomplete status of the program at this time, no additional libraries are required.

Future versions will include SDL 2 as a requirement.

## Intended Minimum Requirements

- Windows 7 or newer (64-bit), Linux 4.4 or newer, Mac OS X 10.9 or newer (64-bit)
- Intel Core 2 Duo or better
- 2 GB of RAM
- 2 GB of Hard Disk Space
- Graphics Card with a minimum resolution of 800*600

## Compiler Requirements

- GCC 4.7 or newer (i.e. CodeBlocks 13.12 or newer)
- Visual Studio 2013 or newer
