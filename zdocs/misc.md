# ROM

The Old World ROM is always 4 megabytes (MB). The first three MB are reserved for the 68k code, while the last MB is for the PowerPC boot-up code.

# BMac

The BMac is an ethernet controller.

The max frame size is 0x5EE bytes.

It resides on 0xF3011000, with Writing DMA on 0xF3008200 and Reading DMA on 0xF3008300.

Swim3 is located at 0xF3015000.

# Serial

For serial, it replicates the functionality of a Zilog ESCC. There are two different ports - one located at 0xF3013000 for the printer, and the other at 0xF3013020 for the modem.

# DBDMA

The Description-Based Direct Memory Access relies on memory-based descriptions, minimizing CPU interrupts.

# SWIM 3

The SWIM 3 (Sanders-Wozniak integrated machine 3) is the floppy drive disk controller. As can be inferred by the name, the SWIM III chip is the improvement of a combination of floppy disk driver designs by Steve Wozniak (who worked on his own floppy drive controller for early Apple computers) and Wendell B. Sander (who worked on an MFM-compatible IBM floppy drive controller). 

The SWIM chip is resided on the logic board. It sits between the I/O controller and the floppy disk connector. Its function is to translate the I/O commands to specialized signals to drive the floppy disk drive, i.e. disk spinning speed, head position, phase sync, etc.

The floppy drives themselves were provided by Sony.

# NVRAM

Mac OS relies on 8 KB of NVRAM at minimum to run properly. It's usually found at IOBase (ex.: 0xF3000000 for Power Mac G3 Beige) + 0x60000.

# Miscellaneous

The Power Mac G3 Beige has an additional register at 0xFF000004, which is dubbed varyingly as the "cpu-id" (by OpenFirmware), the ""systemReg" (display driver) or "MachineID" (platform driver).
