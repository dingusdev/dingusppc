# ROM

The Old World ROM is always 4 megabytes (MB). The first three MB are reserved for the 68k code, while the last MB is for the PowerPC boot-up code.

# Serial

For serial, it replicates the functionality of a Zilog ESCC. There are two different ports - one located at (MacIOBase) + 0x13000 for the printer, and the other at (MacIOBase) + 0x13020 for the modem.

# DBDMA

The Description-Based Direct Memory Access relies on memory-based descriptions, minimizing CPU interrupts.

| Channel           | Number |
|:-----------------:|:------:|
| SCSI0             | 0x0    |
| FLOPPY            | 0x1    |
| ETHERNET TRANSMIT | 0x2    |
| ETHERNET RECIEVE  | 0x3    |
| SCC TRANSMIT A    | 0x4    |
| SCC RECIEVE A     | 0x5    |
| SCC TRANSMIT B    | 0x6    |
| SCC RECIEVE B     | 0x7    |
| AUDIO OUT         | 0x8    |
| AUDIO IN          | 0x9    |
| SCSI1             | 0xA    |

# NCR 53C94

The NCR 53C94 is the SCSI controller.

# Register Map

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

# SWIM 3

The SWIM 3 (Sanders-Wozniak integrated machine 3) is the floppy drive disk controller. As can be inferred by the name, the SWIM III chip is the improvement of a combination of floppy disk driver designs by Steve Wozniak (who worked on his own floppy drive controller for early Apple computers) and Wendell B. Sander (who worked on an MFM-compatible IBM floppy drive controller).

The SWIM chip is resided on the logic board physically and is located at IOBase + 0x15000 in the device tree. It sits between the I/O controller and the floppy disk connector. Its function is to translate the I/O commands to specialized signals to drive the floppy disk drive, i.e. disk spinning speed, head position, phase sync, etc.

Unlike its predecessor, it allowed some DMA capability.

The floppy drives themselves were provided by Sony.

Some New World Macs do have a SWIM 3 driver present, but this normally goes unused due to no floppy drive being connected.

# NVRAM

Mac OS relies on 8 KB of NVRAM at minimum to run properly. It's usually found at IOBase (ex.: 0xF3000000 for Power Mac G3 Beige) + 0x60000.

# PMU

| Command Name     | Number | Functionality                |
|:----------------:|:------:|:----------------------------:|
| PMUpMgrADB       | 0x20   | Send ADB command             |
| PMUpMgrADBoff    | 0x21   |
| PMUxPramWrite    | 0x32   |
| PMUtimeRead      | 0x38   |
| PMUxPramRead     | 0x3A   |
| PMUmaskInts      | 0x70   |
| PMUreadINT       | 0x78   |
| PMUPmgrPWRoff    | 0x7E   |
| PMUResetCPU      | 0xD0   |

# Miscellaneous

The Power Mac G3 Beige has an additional register at 0xFF000004, which is dubbed varyingly as the "cpu-id" (by Open Firmware), the ""systemReg" (display driver) or "MachineID" (platform driver).
