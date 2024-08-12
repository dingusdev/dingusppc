# ROM

The Old World ROM is always 4 megabytes (MB). The first three MB are reserved for the 68k code, while the last MB is for the PowerPC boot-up code.

New World ROMs are 1 MB stubs containing OpenFirmware and some basic drivers, but has an additional ROM stored on the Mac's hard disk to provide Toolbox functionality. The ROMs stored on the Mac's hard disk also had updates distributed.

Within Apple, the project to overhaul Mac OS ROM code from separate portable, low-end, and high-end branches into a single codebase was called SuperMario.

# Serial

For serial, it replicates the functionality of a Zilog ESCC. There are two different ports - one located at (MacIOBase) + 0x13000 for the printer, and the other at (MacIOBase) + 0x13020 for the modem.

# NCR 53C94

The NCR 53C94 is the SCSI controller.

## Register Map

| Offset | Read functionality       |Write functionality        |
|:------:|:------------------------:|:-------------------------:|
| 0x0    | Transfer counter LSB     | Transfer counter LSB      |
| 0x1    | Transfer counter MSB     | Transfer counter MSB      |
| 0x2    | FIFO                     | FIFO                      |
| 0x3    | Command                  | Command                   |
| 0x4    | Status                   | Destination Bus ID        |
| 0x5    | Interrupt                | Select/reselect timeout   |
| 0x6    | Sequence step            | Synch period              |
| 0x7    | FIFO flags/sequence step | Synch offset              |
| 0x8    | Configuration 1          | Configuration 1           |
| 0x9    |                          | Clock conversion factor   |
| 0xA    |                          | Test mode                 |
| 0xB    | Configuration 2          | Configuration 2           |
| 0xC    | Configuration 3          | Configuration 3           |
| 0xF    |                          | Reserve FIFO Byte (Cfg 2) |

# NVRAM

Mac OS relies on 8 KB of NVRAM at minimum to run properly. It's usually found at IOBase (ex.: 0xF3000000 for Power Mac G3 Beige) + 0x60000.

On a physical machine, one has to hold the Command/Apple, Option, P and R keys together. However, using DingusPPC, one can simply delete the nvram.bin file instead.

# PMU

| Command Name     | Number | Functionality                |
|:----------------:|:------:|:----------------------------:|
| PMUpowerCntl     | 0x10   | Power Plane/Clock Control    |
| kPMUpowerRead    | 0x18   | Turns ADB auto-polling off   |
| PMUpMgrADB       | 0x20   | Send ADB command             |
| PMUpMgrADBoff    | 0x21   | Turns ADB auto-polling off   |
| PMUxPramWrite    | 0x32   |
| PMUtimeRead      | 0x38   | Read time from clock
| PMUxPramRead     | 0x3A   |
| PMUmaskInts      | 0x70   |
| PMUreadINT       | 0x78   |
| PMUPmgrPWRoff    | 0x7E   |
| PMUsleepReq      | 0x7F   |
| PMUResetCPU      | 0xD0   |
| PMUwritePmgrRAM  | 0xE0   |
| PMUdownloadStatus| 0xE2   |
| PMUreadPmgrRAM   | 0xE8   |

# DACula

This video RAMDAC appears to be exclusive to the Power Mac 7200.

# USB

Support is only present in New World Macs, despite the presence of strings in the Power Mac G3 Beige ROM. Most Macs support 1.1, with 2.0 support present in G5 Macs. Both it and Firewire follow the Open Host Controller Interface (OHCI) standard.

# FireWire

Present in several New World Macs is a FireWire controller. Mac OS Classic normally only supports FireWire 400.

# Miscellaneous

* In order for the mouse to move, it generally needs to use the Vertical Blanking Interrupt (VBL) present on the video controller. However, the Pippin instead uses a virtual timer task to accomplish this, as there is a bug that prevents the VBL from working in the Taos graphics controller.

* The Power Mac G3 Beige has an additional register at 0xFF000004, which is dubbed varyingly as the "cpu-id" (by Open Firmware), the ""systemReg" (display driver) or "MachineID" (platform driver).
