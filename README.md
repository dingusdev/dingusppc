# DingusPPC

Written by divingkatae and maximumspatium

Be warned the program is highly unfinished and could use a lot of testing. Any feedback is welcome.

## Philosophy of Use

While many other PowerPC emus exist (PearPC, Sheepshaver), none of them currently attempt emulation of PowerPC Macs accurately (except for QEMU).

This program aims to not only improve upon what Sheepshaver, PearPC, and other PowerPC Mac emulators have done, but also to provide a better debugging environment. This currently is designed to work best with PowerPC Old World ROMs, including those of the Power Mac 6100, 7200, and G3 Beige.

## Implemented Features

This emulator has a debugging environment, complete with a disassembler. We also have implemented enough to allow Open Firmware to boot, going so far as to allow audio playback of the boot-up jingles.

## How to Use

This program currently uses the command prompt to work.

There are a few command line arguments one must enter when starting the program.

```
-r, --realtime
```

Run the emulator in runtime.

```
-d, --debugger
```

Enter the interactive debugger.

```
-b, --bootrom TEXT:FILE
```

Specifies the Boot ROM path (optional; looks for bootrom.bin by default)

```
-m, --machine TEXT
```

Specify machine ID (optional; will attempt to determine machine ID from the boot rom otherwise)

As of now, the most complete machines are the Power Mac 6100 (SCSI emulation in progress) and the Power Mac G3 Beige (SCSI + ATA emulation in progress, No ATI Rage acceleration).

## How to Compile

You need to install development tools first.

At least, a C++20 compliant compiler and [CMake](https://cmake.org) are required.

You will also have to recursive clone or run
```
git submodule update --init --recursive
```

This is because the CubeB, Capstone, and SDL2 modules are not included by default.

For SDL2, Linux users may also have run:

```
sudo apt install libsdl2-dev
```

 CLI11 and loguru are already included in the thirdparty folder and compiled along with the rest of DingusPPC.

For example, to build the project in a Unix-like environment, you will need to run
the following commands in the OS terminal:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make dingusppc
```
You may specify another build type using the variable CMAKE_BUILD_TYPE.

For Raspbian, you may also need the following command:
```
sudo apt install doxygen graphviz
```

## Testing

DingusPPC includes a test suite for verifying the correctness of its PowerPC CPU
emulation. To build the tests, use the following terminal commands:
```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DPPC_BUILD_PPC_TESTS=True ..
make testppc
```

## Intended Minimum Requirements

- Windows 7 or newer (64-bit), Linux 4.4 or newer, Mac OS X 10.9 or newer (64-bit)
- Intel Core 2 Duo or better
- 2 GB of RAM
- 2 GB of Hard Disk Space
- Graphics Card with a minimum resolution of 800*600

## Compiler Requirements

- Any C++20 compatible compiler
