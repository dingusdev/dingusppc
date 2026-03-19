# DingusPPC

Written by divingkatae and maximumspatium

Be warned the program is highly unfinished and could use a lot of testing. Any feedback is welcome.

## Philosophy of Use

While many other PowerPC emus exist (PearPC, Sheepshaver), none of them currently attempt emulation of PowerPC Macs
accurately (except for QEMU).

This program aims to not only improve upon what Sheepshaver, PearPC, and other PowerPC Mac emulators have done, but
also to provide a better debugging environment. This currently is designed to work best with PowerPC NuBus and Old
World ROMs, including those of the Power Mac 6100, 7200, and G3 Beige.

## Implemented Features

Several machines have been implemented to varying degrees, like many Old World PowerPC Macs, early New World
PowerPC Macs, and the Pippin.

This emulator has a debugging environment, complete with a disassembler. 

## How to Use

This program currently uses the command prompt to work.

There are a few command line arguments one can enter when starting the program.

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

Specifies the Boot ROM path. It otherwise looks for bootrom.bin.

```
-m, --machine TEXT
```

Specify machine ID. Otherwise, the emulator will attempt to determine machine ID from the boot rom otherwise.

As of now, the most complete machines are the Power Mac 6100, the Power Mac 7500, and the Power Mac G3 Beige.

To go into to the debugger regardless of how you started the emulator, press Control and C on the terminal window.

## How to Compile

You need to install development tools first.

At minimum, a C++20 compliant compiler and [CMake](https://cmake.org) are required.

Clone the repository using the appropriate command:

```
git clone https://github.com/dingusdev/dingusppc
```

If this is from a mirror, replace the argument with the source you want to use instead.

You will also have to recursive clone or run

```
git submodule update --init --recursive
```

This is because the CubeB, Capstone, and SDL2 modules are not included by default.

For SDL2, Linux users may also have to run:

```
sudo apt install libsdl2-dev
```

macOS users can use Homebrew or MacPorts to install SDL2.

CLI11 and loguru are already included in the thirdparty folder and compiled along with the rest of DingusPPC.

For Raspbian, you may also need the following command:

```
sudo apt install doxygen graphviz
```

To build the project in a Unix-like environment, create a build folder, change the directory to the build folder,
and use `cmake` to create the `Makefile` files in that build folder.
Use `make` to do the building. You don't need to execute `cmake` again unless you add/remove/change options or
source files.

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

You may specify another build type using the variable `CMAKE_BUILD_TYPE`. 
Each build type should have its own build folder.

```
mkdir build-debug
cd build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

## Testing

DingusPPC includes a test suite for verifying the correctness of its PowerPC CPU
emulation. To build the tests, use the following terminal commands:

```
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release -DDPPC_BUILD_PPC_TESTS=True ..
make testppc
```

## Intended Minimum Requirements

- Windows 7 or newer (64-bit), Linux 4.4 or newer, Mac OS X 10.9 or newer (64-bit)
- Intel Core 2 Duo or better
- 2 GB of RAM
- 2 GB of Hard Disk Space
- Graphics Card with a minimum resolution of 800*600
