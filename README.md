# DingusPPC by divingkatae

Be warned the program is highly unfinished and could use a lot of testing. Any feedback is welcome.

## PHILOSOPHY OF USE
 
Sheepshaver, while technically impressive, is becoming harder to compile and run. While many other PowerPC emus exist, none of them currently attempt emulation of PPC Macs (except for QEMU). 

This program aims to not only improve upon what Sheepshaver has done, but also to provide a better debugging environment. This currently is designed to work best with PowerPC Old World ROMs, 
including those of the PowerMac G3 Beige.
 
## HOW TO USE

This program uses the command prompt to work.

There are a few command line arguments one can enter when starting the program.

-fuzzer

Processor fuzzer, very unfinished.

-realtime

Run the emulator in runtime.

-loadelf

Load an ELF file into memory.

-debugger

Enter the interactive debugger.

-stepp

Execute a page of opcodes (256 instructions at a time).

-playground

allows users to enter 32-bit hex opcodes to mess with the PPC processor. 

## HOW TO COMPILE 
 
Run sudo apt-get install build-essential
 
From the terminal, run makefile.
 
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
