# VIRTUAL MEMORY MAP

| Address       | Area                                   |
|:-------------:|:--------------------------------------:|
| 0x5FFFE000    | InfoRecord                             |
| 0x5FFFEFF0    | NKSystemInfo                           |
| 0x68000000    | Motorola 68K Emulator (0x100000 bytes) |
| 0x68060000    | Emulator Code                          |
| 0x68080000    | Opcode Dispatch Table                  |
| 0x68FFE000    | KernelData                             |
| 0x68FFF000    | EmulatorData                           |
| 0xFF800000    | Open Firmware                          |
| 0xFFF0C000    | HardwarePriv                           |

# PHYSICAL MEMORY MAP

## NuBus Power Macs

(Sourced Heavily from: http://mess.redump.net/mess/driver_info/mac_technical_notes)

|Starting Address|Ending Address| Area                         |
|:--------------:|:------------:|:----------------------------:|
| 0x00000000     | 0x3FFFFFFF   | Main Memory                  |
| 0x40000000     | 0x4FFFFFFF   | ROM Mirrors                  |
| 0x50000000     | 0x5FFFFFFF   | IO Devices                   |
| 0x5FFFFFFC     |              | "cpuid", really a Machine ID |
| 0x60000000     | 0xEFFFFFFF   | NuBus "super slot" space     |
| 0xF1000000     | 0xFFBFFFFF   | NuBus "standard slot" space  |
| 0xFFC00000     | 0xFFFFFFFF   | ROM                          |

### IO Bus

| Address       | Area               |
|:-------------:|:------------------:|
| 0x50F00000    | IO Base Address    |
| 0x50F04000    | SCC                |
| 0x50F14000    | Sound Chip (AWACS) |
| 0x50F24000    | CLUT Control       |
| 0x50F28000    | Video Control      |
| 0x50F2A000    | Interrupt Control  |


## PCI Power Macs

### Main Memory
* 0x00000000 - 0x7FFFFFFF
    Mac OS

* 0x00400000 - Open Firmware

### PCI/Device Memory Area 0x80000000 - 0xFF000000

* 0x81000000 - Video Display Device (normally)

* 0xF3000000 -
    Mac OS I/O Device area

* 0xF3008000 - 0xF3008FFF - DMA Channels

* 0xF3008000 - SCSI DMA
* 0xF3008100 - Floppy DMA
* 0xF3008200 - Ethernet transmit DMA
* 0xF3008300 - Ethernet receive DMA
* 0xF3008400 - SCC channel A transmit DMA
* 0xF3008500 - SCC channel A receive DMA
* 0xF3008600 - SCC channel B transmit DMA
* 0xF3008700 - SCC channel B receive DMA
* 0xF3008800 - Audio out DMA
* 0xF3008900 - Audio in DMA

* 0xF3009000 - ATI Mach 64 video card

* 0xF3010000 - SCSI device registers (0x100 bytes)
* 0xF3011000 - MACE (serial) device registers (0x100 bytes)
* 0xF3012000 - SCC compatibility port (?) (0x100 bytes)
* 0xF3013000 - SCC MacRISC port (Serial for 0x20, then Modem for 0x20, with remaining 0xC0 unknown)
* 0xF3014000 - AWAC (Audio) chip device registers
* 0xF3015000 - SWIM3 (floppy controller) device registers
* 0xF3016000 - pseudo VIA1 device registers
* 0xF3017000 - pseudo VIA2 device registers

* 0xF3020000 - Heathrow ATA

* 0xF8000000 - Hammerhead memory controller registers (0x1000000 bytes)

* 0xFE010000 - 53C875 Hard Drive Controller

* 0xFE000000 - Grackle Low/Base
* 0xFEC00000 - Grackle CONFIG_ADDR (0x4 bytes, all redirected to 0xXXXXXCF8)
* 0xFEE00000 - Grackle CONFIG_DATA (0x4 bytes, all redirected to 0xXXXXXCFF)

### ROM / Misc Area (0xFF000000 - 0xFFFFFFFF)
* 0xFF000000 - ?

  *  0xFF000004 - "cpuid", really a machine ID

* 0xFFC00000 - 0xFFFFFFFF
    Mac OS ROM Area

  * 0xFFC00000 - 0xFFEFFFFF - 68k Code Area

    (below addresses apply to Old World ROMs)

  * 0xFFF00100 - Reset Area (where the ROM begins executing)
  * 0xFFF10000 - Nanokernel Code
  * 0xFFF20000 - HW Init