# Open Firmware in Power Macintosh

*compiled from various sources by Max Poliakovski.*

[Open Firmware](https://en.wikipedia.org/wiki/Open_Firmware) is a platform-independent
boot firmware architecture covered by an IEEE standard.

All Power Macintosh computers run Open Firmware except the very first generation
that uses [Nubus](https://en.wikipedia.org/wiki/NuBus) instead of
[PCI](https://en.wikipedia.org/wiki/Peripheral_Component_Interconnect).

Open Firmware is used to perform hardware identification and initialization during
the booting process after power-on. It also provides a platform-independent
description of the attached devices available for operating systems.
In this respect, Open Firmware can be compared with [BIOS](https://en.wikipedia.org/wiki/BIOS),
widely used in the PC world.

Being based upon the [Forth programming language](https://en.wikipedia.org/wiki/Forth_(programming_language)),
Open Firmware offers an operating system, an interactive environment as well as a
programming language in one package. Its shell can be used as well by users for
controlling the boot environment as by developers for developing and debugging device
drivers.

This document focuses on various aspects of Apple's Open Firmware implementation
as found in various Power Macintosh models.

## ROM Layout

### Old World ROMs

There's a 3 MiB Classic section and a 1 MiB PowerPC section.
Use [tbxi](https://github.com/elliotnunn/tbxi) to split the ROM into parts.

### New World ROMs

The ROM only contains the 1MiB PowerPC section. The ROM is divided into sections:

| Name                     | Firmware Updater Section Name | Start      | Size       | Comments |
|--------------------------|-------------------------------|------------|------------|----------|
| Start                    | bb                            | 0xfff00000 | 0x00003f00 | Exception Vectors. |
| Recovery                 | rec                           | 0xfff08000 | 0x00078000 | Usually same as ROM Image. |
| Rom Image                | boot                          | 0xfff80000 | 0x00080000 | Usually same as Recovery. |
| System Configuration	   | sys                           | 0xfff03f00 | 0x00000080 | Contains Model specific info. Doesn't exist in early New World Macs. Three bytes in the System Configuration comprise the Mac Product ID. |
|                          | tst                           | 0xfff03f80 | 0x00000080 | Contains unit specific info: Serial Number, MAC address. |
| NVRAM                    | nv                            | 0x00004000 | 0x00004000 | Contains two copies of NVRAM, 8K each. |

At the beginning of the Recovery and Rom Image is a header listing subsections. The list differs depending on the firmware version.

The earliest New World ROMs include all of Open Firmware (main fcode image and driver fcode images) in the Open Firmware subsection. RTAS is an fcode image.

| Offset | Index | @startvec address field | @startvec size field |
|--------|-------|-------------------------|----------------------|
| 0x00   |       | >dir.inst0              | >dir.inst1           |
| 0x08   |       | >dir.filler0            | >dir.filler1         |
| 0x10   | 0     | >dir.HWINIT             | >dir.HWINIT-size     |
| 0x18   | 1     | >dir.NUB                | >dir.NUB-size        |
| 0x20   | 2     | >dir.OF                 | >dir.OF-size         |
| 0x28   | 3     | >dir.RTAS               | >dir.RTAS-size       |
| 0x30   | 4     | >dir.RTAS-LDR           | >dir.RTAS-LDR-size   |
| 0x38   | 5     | >dir.RTAS-BE            | >dir.RTAS-BE-size    |
| 0x40   | 6     | >dir.RTAS-LE            | >dir.RTAS-LE-size    |
| 0x48   | 7     | >dir.BOOT-BEEP          | >dir.BOOT-BEEP-size  |

Newer ROMs did away with the RTAS stuff:

| Offset | Index | @startvec address field | @startvec size field |
|--------|-------|-------------------------|----------------------|
| 0x00   |       | >dir.inst0              | >dir.inst1           |
| 0x08   |       | >dir.filler0            | >dir.filler1         |
| 0x10   | 0     | >dir.HWINIT             | >dir.HWINIT-size     |
| 0x18   | 1     | >dir.NUB                | >dir.NUB-size        |
| 0x20   | 2     | >dir.OF                 | >dir.OF-size         |
| 0x28   | 3     | >dir.unused0            | >dir.unused0-size    |
| 0x30   | 4     | >dir.unused1            | >dir.unused1-size    |
| 0x38   | 5     | >dir.unused2            | >dir.unused2-size    |
| 0x40   | 6     | >dir.unused3            | >dir.unused3         |
| 0x48   | 7     | >dir.BOOT-BEEP          | >dir.BOOT-BEEP-size  |

Starting approximate 2002-11-11 and later, this is the ROM header or first part of @startvec. Different Macs with the same firmware version may include different driver fcode images if the ROM is not large enough to contain all drivers.

| Offset | Index | @startvec address field | @startvec size field |
|--------|-------|-------------------------|----------------------|
| 0x00   |       | >dir.inst0              | >dir.inst1           |
| 0x08   |       | >dir.filler0            | >dir.filler1         |
| 0x10   |  0    | >dir.HWINIT             | >dir.HWINIT-size     |
| 0x18   |  1    | >dir.OF                 | >dir.OF-size         |
| 0x20   |  2    | >dir.Driver1            | >dir.Driver1-size    |
| 0x28   |  3    | >dir.Driver2            | >dir.Driver2-size    |
| 0x30   |  4    | >dir.Driver3            | >dir.Driver3-size    |
| 0x38   |  5    | >dir.Driver4            | >dir.Driver4-size    |
| 0x40   |  6    | >dir.Driver5            | >dir.Driver5-size    |
| 0x48   |  7    | >dir.Driver6            | >dir.Driver6-size    |
| 0x50   |  8    | >dir.Driver7            | >dir.Driver7-size    |
| 0x58   |  9    | >dir.Driver8            | >dir.Driver8-size    |
| 0x60   | 10    | >dir.Driver9            | >dir.Driver9-size    |
| 0x68   | 11    | >dir.Driver10           | >dir.Driver10-size   |
| 0x70   | 12    | >dir.Driver11           | >dir.Driver11-size   |
| 0x78   | 13    | >dir.Driver12           | >dir.Driver12-size   |
| 0x80   | 14    | >dir.Driver13           | >dir.Driver13-size   |
| 0x88   | 15    | >dir.Driver14           | >dir.Driver14-size   |
| 0x90   | 16    | >dir.Driver15           | >dir.Driver15-size   |
| 0x98   | 17    | >dir.BOOT-BEEP          | >dir.BOOT-BEEP-size  |

The latest firmware versions have driver fcode images in one subsection and each driver fcode image has a name and is individually compressed.

| Offset | Index | @startvec address field | @startvec size field  |
|--------|-------|-------------------------|-----------------------|
| 0x00   |       | >dir.inst0              | >dir.inst1            |
| 0x08   |       | >dir.filler0            | >dir.filler1          |
| 0x10   | 0     | >dir.HWINIT             | >dir.HWINIT-size      |
| 0x18   | 1     | >dir.OF                 | >dir.OF-size          |
| 0x20   | 2     | >dir.dV1.Drivers        | >dir.dV1.Drivers-size |
| 0x28   | 3     | >dir.BOOT-BEEP          | >dir.BOOT-BEEP-size   |

The offset for each of the parts is at least 4 byte aligned and has the least significant bit set to 1 if it is lzss compressed.
Only early New World Macs (B&W G3, iMac G3 233-333, PowerBook G3 Lombard) have the RTAS (fcode) and RTAS-BE (executable) parts.
There don't appear to be any ROMs that have NUB, RTAS_LDR, or RTAS-LE.
RTAS is not included in later ROMs so the four parts 3-6 are always 0x00000000 for offset and size.
Since Recovery is smaller than Rom Image, some newer ROMs have BOOT-BEEP of the Recovery section pointing to BOOT-BEEP of the Rom Image section.

The `>dir.*` fields in the ROM are for the offset and size of the compressed parts. The `>dir.*` fields also exist in RAM starting from `@startvec` but they store the address and size of the parts uncompressed.

## Open Firmware Versions

### NuBus Power Macs

NuBus Power Macs do not have Open Firmware.
However, Copland has a version of Open Firmware 2.0 for NuBus Power Macs that is loaded from disk at startup.
The default output device is `ttyb` (the printer port) instead of `ttya` because the serial port is used for debugging.

| ROM dump/Machine                     | BootROM codename | ROM version | OF version    | OF location     | OF image size |
|--------------------------------------|------------------|-------------|---------------|-----------------|---------------|
| Power Macintosh 6100/7100/8100       | PDM              | 077d.20f2   | 2.0 Copland   | opfw resource   | < 0x20000     |
| Power Macintosh 7100 newer           | PDM              | 077d.23f1   | 2.0 Copland   | opfw resource   | < 0x20000     |

### Old World Macs

| ROM dump/Machine                     | BootROM codename | ROM version | OF version    | OF image offset | OF image size |
|--------------------------------------|------------------|-------------|---------------|-----------------|---------------|
| Power Macintosh 7200/7500/8500/9500  | TNT              | 077d.28f1   | 1.0.5         | 0x330000        | 0x20000       |
| Power Macintosh 7200/7500/8500/9500 v2, SuperMac S900 | TNT | 077d.28f2 | 1.0.5       | 0x330000        | 0x20000       |
| Apple Network Server ANS 500 & 700   | TNT              | 077d.28f2   | 1.1.22        | 0x330000        | 0x20000       |
| Power Macintosh 5400 & 6400          | Alchemy          | 077d.29f1   | 2.0           | 0x330000        | 0x20000       |
| Bandai Pippin (Kinka Dev)            | Pip              | 077d.2cc6   | 1.0.5         | 0x330000        | 0x20000       |
| Bandai Pippin (Kinka 1.0)            | Pip              | 077d.2cf2   | 1.0.5         | 0x330000        | 0x20000       |
| Bandai Pippin (Kinka 1.2)            | Pip              | 077d.2cf5   | 1.0.5         | 0x330000        | 0x20000       |
| Bandai Pippin (Kinka 1.3)            | Pip              | 077d.2cf8   | 1.0.5         | 0x330000        | 0x20000       |
| Power Macintosh 7300/7600/8600/9600  | TNT              | 077d.34f2   | 1.0.5         | 0x330000        | 0x20000       |
| Power Macintosh 8600 & 9600 (v2)     | TNT              | 077d.34f5   | 1.0.5         | 0x330000        | 0x20000       |
| Power Macintosh 6500 & TAM           | Gazelle          | 077d.35f2   | 2.0.3         | 0x330000        | 0x20000       |
| PowerBook 3400c                      | PSX              | 077d.36f1   | 2.0.1         | 0x330000        | 0x20000       |
| PowerBook G3 Kanga                   | PSX              | 077d.36f5   | 2.0.1         | 0x330000        | 0x20000       |
| Power Macintosh 4400/7220            | Zanzibar         | 077d.3af2   | 2.0.2         | 0x330000        | 0x20000       |
| PowerBook G3 Wallstreet              | GRX              | 077d.41f5   | 2.0.1         | 0x330000        | 0x20000       |
| PowerBook G3 Wallstreet PDQ          | GRX              | 077d.41f6   | 2.0.1         | 0x330000        | 0x20000       |
| Power Macintosh G3 desktop (rev A)   | Gossamer         | 077d.40f2   | 2.0f1         | 0x320000        | 0x30000       |
| Power Macintosh G3 (rev B)           | Gossamer         | 077d.45f1   | 2.0f1         | 0x320000        | 0x30000       |
| Power Macintosh G3 v3 (rev C)        | Gossamer         | 077d.45f2   | 2.4           | 0x320000        | 0x30000       |
| Power Macintosh G3 (rev D)           | Gossamer         | 077d.45f3   | 2.4           | 0x320000        | 0x30000       |

### New World Macs

| Date                | OF version | Machine                                 |
|---------------------|------------|-----------------------------------------|
| 1998-08-06 12:04:16 | 3.0.0f8    | iMac                                    |
| 1998-09-17 15:19:23 | 3.0.0b2    | iMac                                    |
| 1998-12-02 10:25:41 | 3.0.0f3    | iMac                                    |
| 1999-02-03 16:45:21 | 3.0.0f9    | iMac                                    |
| 1999-03-05 21:14:19 | 3.0.0f10   | iMac                                    |
| 1999-04-01 16:47:10 | 3.0.0f1    | iMac                                    |
| 1999-04-06 10:21:59 | 3.1.0f1    | PowerBook G3 Bronze Keyboard (Lombard)  |
| 1999-04-09 13:57:32 | 3.1.1f4    | Power Mac G3 (Blue & White) Yosemite    |
| 1999-08-19 21:36:38 | 3.1.2f2    | Power Mac G4 (PCI Graphics) Yikes       |
| 1999-04-23 14:31:03 | 3.1.3f2    | iMac G3 (233 MHz) (Bondi Blue)          |
| 1999-07-16 12:15:09 | 3.1.3f3    | iMac G3 (266/333 MHz)                   |

Newer firmware versions occasionally received firmware updates which could be applied to multiple models so it doesn't make sense to associate a model to a firmware version.
The list below includes a source for each firmware version.
Versions 4.x.x are for 32-bit Power Macs. Version 5.x.x are for 64-bit Power Macs.

| Date                | OF version | Machine                                 |
|---------------------|------------|-----------------------------------------|
| 2000-02-18 09:44:35 | 3.2.4f1    | Firmware Updater 3.2.4f1                |
| 2000-02-18 09:44:51 | 3.2.4f1    | Power Mac G4 (AGP Graphics) Sawtooth    |
| 2000-08-13 10:41:50 | 3.2.7f2    | Firmware Updater 3.2.7f2                |
| 2000-06-13 ??:??:?? | 3.3.0f3    | Summer 2000 iMac DV+ (450MHz)           |
| 2000-07-10 17:11:57 | 3.3.2f1    | Power Mac G4 Cube                       |
| 2000-12-04 19:29:52 | 4.1.0f1    | Power Mac G4 (Digital Audio)            |
| 2001-03-20 17:20:48 | 4.1.7f4    | Firmware Updater 4.1.7f4                |
| 2001-03-20 17:20:48 | 4.1.7f4    | iBook G3                                |
| 2001-03-21 11:49:53 | 4.1.8f5    | Firmware Updater 4.1.8f5                |
| 2001-03-21 11:49:53 | 4.1.8f5    | PowerBook G3 (FireWire) Pismo           |
| 2001-09-14 13:18:04 | 4.1.9f1    | Firmware Updater 4.1.9f1                |
| 2001-09-14 13:18:04 | 4.1.9f1    | iMac G3 (Slot Loading)                  |
| 2001-09-14 13:18:04 | 4.1.9f1    | Power Mac G4 Cube                       |
| 2001-08-01 11:14:42 | 4.2.3f1    | Power Mac G4 (Quicksilver)              |
| 2001-08-16 22:19:35 | 4.2.5f1    | Power Mac G4 (Quicksilver)              |
| 2001-09-12 17:47:15 | 4.2.7f1    | iBook G3 600 (Late 2001 Snow)           |
| 2001-10-11 14:12:47 | 4.2.8f1    | Firmware Updater 4.2.8f1                |
| 2001-10-11 14:12:47 | 4.2.8f1    | Power Mac G4 (AGP Graphics) Sawtooth    |
| 2001-10-11 14:12:47 | 4.2.8f1    | Power Mac G4 (Gigabit Ethernet)         |
| 2001-10-11 14:12:47 | 4.2.8f1    | Power Mac G4 (Digital Audio)            |
| 2001-11-20 14:31:20 | 4.2.9f1    | Firmware Updater 4.2.9f1                |
| 2001-12-06 17:15:39 | 4.3.2f1    | iBook G3 600 14-Inch (Early 2002 Snow)  |
| 2002-04-08 22:45:14 | 4.4.0f1    | iMac G4                                 |
| 2002-05-19 09:26:24 | 4.4.1f1    | iMac G4                                 |
| 2002-07-23 13:58:33 | 4.4.5f3    | iMac G4 17 inch                         |
| 2002-09-30 10:24:31 | 4.4.8f2    | Firmware Updater 4.4.8f2                |
| 2002-09-30 10:24:31 | 4.4.8f2    | Power Mac G4 (Mirrored Drive Doors)     |
| 2002-11-11 16:06:29 | 4.5.4f1    | iBook G3 (Snow)                         |
| 2003-01-15 15:14:50 | 4.5.7f1    | Power Mac G4 (FW 800)                   |
| 2003-01-13 22:37:37 | 4.5.8f1    | iMac G4/1.0 17" (Flat Panel)            |
| 2003-02-20 13:52:27 | 4.6.0f1    | Power Mac G4 (FW 800)                   |
| 2003-02-18 14:40:21 | 4.6.2f1    | PowerBook G4 1.0 GHz 17 inch (Aluminum) |
| 2003-03-15 10:16:09 | 4.6.4f1    | iBook G3 (Snow)                         |
| 2003-08-22 13:45:20 | 4.6.8f4    | iMac G4/1.0 17" (Flat Panel)            |
| 2003-09-04 13:39:26 | 4.7.1f1    | PowerBook G4 (15-inch FW800) (Aluminum) |
| 2003-11-21 17:41:48 | 5.1.4f0    | Firmware Updater 5.1.4f0                |
| 2004-09-21 11:58:53 | 5.1.5f2    | Firmware Updater 5.1.5f2                |
| 2004-09-21 11:58:53 | 5.1.5f2    | Power Mac G5 1.6 (PCI)                  |
| 2004-04-06 16:22:09 | 4.8.5f0    | iBook G4 (Early 2004)                   |
| 2004-05-07 11:43:54 | 4.8.6f0    | PowerBook G4 1.5 17" (Al)               |
| 2004-08-25 15:11:09 | 5.1.7f1    | Firmware Updater 5.1.7f1                |
| 2004-09-08 11:50:58 | 5.2.2f2    | Power Macintosh G5 1.8 (PCI)            |
| 2004-09-23 16:13:38 | 4.8.7f1    | iBook G4                                |
| 2004-10-26 16:30:32 | 5.1.8f7    | Firmware Updater 5.1.8f7                |
| 2004-10-26 16:30:32 | 5.1.8f7    | Power Mac G5                            |
| 2005-01-21 10:51:16 | 4.9.1f1    | PowerBook G4 15 inch Early 2005 A1106   |
| 2005-03-23 14:22:23 | 4.8.9f4    | Mac mini G4                             |
| 2005-07-05 11:14:11 | 4.9.3f0    | iBook G4 14-Inch (Mid-2005 - Opaque)    |
| 2005-07-12 16:57:27 | 4.9.4f1    | Mac mini G4 1.5GHz Radeon 9200          |
| 2005-09-22 16:17:32 | 4.9.5f3    | PowerBook G4 DLSD                       |
| 2005-09-30 15:31:03 | 5.2.7f1    | Power Mac G5 Quad                       |
| 2005-10-05 16:45:50 | 4.9.6f0    | PowerBook G4 DLSD                       |
| 2005-10-05 16:45:50 | 4.9.6f0    | PowerBook G4 DLSD (17 inch)             |

## Packages

| Package Name   | Purpose                                       | Versions |
|----------------|-----------------------------------------------|----------|
| obp-tftp       | OpenBoot PROM with tftp                       | 1.0.5+   |
| aix-boot       |                                               | 1.0.5+   |
| xcoff-loader   |                                               | 1.0.5+   |
| mac-files      | Handle hard drives formatted with HFS         | 1.0.5+   |
| mac-parts      | Find a partition with a Mac OS installed      | 1.0.5+   |
| fat-files      | Handle hard drives formatted with FAT(16?)    | 1.0.5+   |
| iso-9660-files | Handle CD ROM images formatted with ISO 9660  | 1.0.5+   |
| telnet         |                                               | 3.0+     |


## Open Firmware image

### Old World Macs

Open Firmware in Old World Macs is stored in the monolithic 4MB ROM. Its hibernated
image is located at offset `0x320000` or `0x330000` from beginning of the ROM.
That corresponds to the physical address `0xFFF20000` or `0xFFF30000`, respectively.

The size of the Open Firmware image varies from 98KB (v1.0.5) to 172KB (v2.4).

Apple's Open Firmware image has the following structure:

| Section type       | Architecture | Relative Size (v1.0.5) | Relative Size (v2.4) |
|--------------------|--------------|------------------------|----------------------|
| OF kernel          | PowerPC      | 26%                    | 18%                  |
| OF main code       | FCode        | 62%                    | 51%                  |
| OF device packages | FCode        | 12%                    | 31%                  |

OF image is wrapped in the COFF container. At the start of the image, a COFF file
header and a section header are located. The following example parses the COFF
headers of the OF image from the Gossamer ROM v3:

```
org 0xFFF20000

; OF COFF file header
dc.w     0x1DF      ; COFF magic
dc.w         1      ; number of sections
dc.l    'Gary'      ; Gary Davidian's signature replaces time and date
dc.l         0      ; ptr to the symbol table
dc.l         0      ; number of entries in the symbol table
dc.w         0      ; size of the optional header
dc.w         0      ; flags

; OF COFF section header
dc.b    '.text', 0, 0, 0    ; section name
dc.l         0      ; physical address
dc.l         0      ; virtual address
dc.l   0x2ADB0      ; section size in bytes (= 171KB)
dc.l      0x3C      ; file offset to section data
dc.l         0      ; file offset to section relocations
dc.l         0      ; file offset to line number entries
dc.w         0      ; number of relocation entries
dc.w         0      ; number of line number entries
dc.l      0x20      ; section flags (this section contains executable code)
```
The section data begins at offset 0x3C right after the COFF section header.
The first part of the section data is a vital OF kernel data structure called `StartVec`.
It contains among other things several important offsets into the OF image
for finding all required OF parts. That's also the location HWInit passes control
to when invoking OF:

```
org 0xFFF2003C

; OF execution starts here
mflr    r11             ; save return address to HWInit in R11
bl      OF_kernel_start ; pass control to OF kernel
                        ; LR will contain physical address of StartVec

; Begin of StartVec structure
; All offsets are from the beginning of the .text section (0xFFF2003C)
FFF20044  dc.l   0x7918 ; offset to the FCode stream of the OF main package
...
FFF2007C  dc.l  0x28C48 ; offset to the last driver in the device packages
...
FFF20084  dc.l   0x7720 ; offset to the last Forth word descriptor of the kernel
```

This StartVec structure in ROM is not to be confused with the `@startvec` structure in RAM.
Open Firmware 1.0.5 and 2.0f1 do not include names for all the fields of `@startvec` but other versions do.
The following is the list of fields from Open Firmware 2.4:

```
0x48D 0008 FF808008 >imagesize            00000000
0x48E 000C FF80800C >'cold2               00000000
0x48F 0010 FF808010 >'map-page            00000000
0x490 0014 FF808014 >'map-io              00000000
0x491 0018 FF808018 >endiango             FF808940
0x492 001C FF80801C >little?              00000000
0x493 0020 FF808020 >swizzle?             00000000
0x494 0024 FF808024 >real?                00000000
0x495 0028 FF808028 >rom-base             FFF2003C
0x496 002C FF80802C >real_base            00400000
0x497 0030 FF808030 >virt_base            FF800000
0x498 0034 FF808034 >real-vt-hd           004DFC00
0x499 0038 FF808038 >restart              FFF201DC
0x49A 003C FF80803C >fcimage              FFF27954
0x49B 0040 FF808040 >fcfiles              FFF48C84
0x49C 0044 FF808044 >lasttoken            00000EB5
0x49D 0048 FF808048 >word-list            FF85D4E8
0x49E 004C FF80804C >'ferror              FF80D0B0 ferror
0x49F 0050 FF808050 >'(poplocals)         FF80A350 (poplocals)
0x4A0 0054 FF808054 >'cold-load           FF80F738 cold-load
0x4A1 0058 FF808058 >'cold-init           FF818350 _cold-init
0x4A2 005C FF80805C >'quit                FF8179A8 quit
0x4A3 0060 FF808060 >'abort               FF811FA0 abort
0x4A4 0064 FF808064 >'syscatch            FF819638 _syscatch
0x4A5 0068 FF808068 >'excp                FF819338 _exception
0x4A6 006C FF80806C >'bp-done             00000000
0x4A7 0070 FF808070 >'step-done           00000000
0x4A8 0074 FF808074 >'mini-nub            00000000
0x4A9 0078 FF808078 >hwinitlr             FFF04D50
0x4AA 007C FF80807C >hwinitiv             00000000
0x4AB 0080 FF808080 >hwinit3              00009000
0x4AC 0084 FF808084 >hwinit4              00009120
0x4AD 0088 FF808088 >hwinit8              FFF03058
0x4AE 008C FF80808C >hwinit9              F3060000
0x4AF 0090 FF808090 >sccac                F3013020
0x4B0 0094 FF808094 >sccad                F3013030
0x4B1 0098 FF808098 >ramsize              30000000
0x4B2 009C FF80809C >cpuclock             0DF092B0
0x4B3 00A0 FF8080A0 >busclock             03FB97A0
0x4B4 00A4 FF8080A4 >tbclock              00FEE5E8
0x4B5 00A8 FF8080A8 >here                 FF85D524
0x4B6 00AC FF8080AC >free-top             FF8E0000
0x4B7 00B0 FF8080B0 >free-bot             FF8D0000
0x4B8 00B4 FF8080B4 >#dsi-ints            00000000
0x4B9 00B8 FF8080B8 >#isi-ints            00000000
0x4BA 00BC FF8080BC >dl-buf               FF8F0800
0x4BB 00C0 FF8080C0 >ttp800               FF8D6000
0x4BC 00C4 FF8080C4 >'cientry             FF809E18 cientry
0x4BD 00C8 FF8080C8 >'cicall              FF829498 cicall
0x4BE 00CC FF8080CC >'my-self             FF80D8D0
0x4BF 00D0 FF8080D0 >'<sc-int>            00409D10
0x4C0 00D4 FF8080D4 >ofregsv              FF8DFA00 ^-720600
0x4C1 00D8 FF8080D8 >ciregsv              FF8DF800
0x4C2 00DC FF8080DC >ofregsr              004DFA00
0x4C3 00E0 FF8080E0 >ciregsr              004DF800
0x4C4 00E4 FF8080E4 >intvectv             FF8DF600
0x4C5 00E8 FF8080E8 >intvectr             004DF600
0x4C6 00EC FF8080EC >regsvalid?           FF808000
0x4C7 00F0 FF8080F0 >ciwords              FF82A3D0
0x4C8 00F4 FF8080F4 >htab                 004E0000
0x4C9 00F8 FF8080F8 >keepbat2?            00000000
0x4CA 00FC FF8080FC >keepbat3?            00000000
0x4CB 0100 FF808100 >rp                   FF800800
0x4CC 0104 FF808104 >dp                   FF800400
0x4CD 0108 FF808108 >fp                   FF801000
0x4CE 010C FF80810C >lp                   FF800C00
0x4CF 0110 FF808110 >ep                   FF801800
0x4D0 0114 FF808114 >ttp                  FF804000
0x4D1 0118 FF808118 >tib                  FF801C00
0x4D2 011C FF80811C >noname               FF802000
0x4D3 0120 FF808120 >dec-ints             00000000
0x4D4 0124 FF808124 >dec-msec             00004141
0x4D5 0128 FF808128 >r13-31               FFF03078
```

The above is produced by this:

```
: dump_fields { addr fcodestart fcodeend ; token }
    cr fcodeend 1+ fcodestart do
        i ." 0x" 3 u.r space
        i get-token drop -> token
        0 token execute 4 u.r space
        addr token execute dup 8 u.r space
        token name.
        #out @ d# 42 < if d# 42 #out @ - spaces then
        @ dup 8 u.r space
        \ check if the field might point to a xt token and if so, output its name
        dup @startvec u> if
            dup dup xt>hdr 4+ dup c@ + 8 + -8 and = if name. else drop then
        else drop then
        cr
    loop
;

: dumpstartvec
    @startvec 48d 4d5 dump_fields ;

dumpstartvec
```

### Open Firmware internals

Apple's Open Firmware contains a small kernel implemented in the native PowerPC code. This kernel performs the following actions:

* Set up memory translations for OF execution.
* Relocate itself from ROM to RAM.
* Initialize the Forth runtime environment which includes a dictionary of words. All words are native PPC code.
* Interpret Forth or fcode which adds more words (with PPC code) and data to the dictionary.
* Process low-level exceptions.

## Open Firmware and the Macintosh boot process

### Old World Macs

1. In response to power coming on, HWInit code in the Power Macintosh ROM performs initialization of the memory controller and the basic I/O facilities as well as some self-testing. After that, the startup chime is played.
2. HWInit passes control to Open Firmware kernel that prepares OF execution from RAM. `cold-load` is executed.
3. `cold-load` begins interpreting of the main fcode image (`@startvec` `>fcimage`) compiling words to PowerPC code and building the **device tree** - a platform-independent description of the attached HW. The main fcode image and driver fcode images (`@startvec` `>fcfiles`) are interpreted from ROM.
4. `cold-load` passes control to `@startvec` `>'cold-init` which tests for snag keys, optionally executes `nvramrc`, and boots the OS (either by returning to HWInit or executing a `boot` command) or enters the Open Firmware command line.
5. OF returns control to HWInit that initializes several low-level data structures required by the Nanokernel.
6. HWInit passes control to the Nanokernel that initializes the native execution environment and the 68k emulator.
7. 68k emulator executes the start-up code in the Macintosh ROM that initializes various managers.
8. The device tree generated by Open Firmware in step 3 is imported by the Expansion Bus Manager initialization code and stored in the **NameRegistry**.
9. An operating system is located and loaded.

### New World Macs

The PowerPC part is similar to Old World Macs except the Open Firmware and OF Driver subsections need to be decompressed into RAM.
For Classic Mac OS, Open Firmware loads and boots a file of type `tbxi`. This is a New World ROM file in the System Folder which replaces the first 3MiB of the Old World ROM.

## Open Firmware bugs

Apple OF is known to contain numerous bugs. The following is a list of some recently discovered bugs, not mentioned elsewhere.

### 1.0.5
- `fill-rectangle` in `/chaos/control` is `( ? color y x w h -- )` but it should be `( color x y w h -- )`.
  This is corrected by Control2.c of BootX which is used to boot Mac OS X.
- `$find` is `( name-str name-len -- xt true | pstr name-str name-len false )` but it should be `( name-str name-len -- xt true | name-str name-len false )`

### 2.0f1, 2.4
- `us` uses `get-usecs` to convert a 64-bit timebase value to a 32-bit microseconds value.
  That's enough for 71 minutes. After that, anything that uses `us` will cause the OF console to become unresponsive requiring a restart.
  In Open Firmware 3.x and later, `get-usecs` returns a 64-bit value.

### 2.4
- string literal buffers `"` on the command line will overflow if the string is more than 256 bytes.

### All 32-bit versions
- `um/mod` produces incorrect results or takes seconds to complete for some dividend/divisor combinations.

  Incorrect results:
  - `0x7eeeeeeeffffffff, 0x7eeeeeef`
  - `0x7eeeeeeffffffff, 0x7eeeeef`

  Too much time:
  - `0x100a000000000, 0x100de`
  - `0x103b000000000, 0x1f090`
  - `0x1fffe00000000, 0x1ffff`
  - `0x20fffe00000000, 0x20ffff`

### Miscellaneous versions
- `get-inherited-property notes.txt` discusses known bugs regarding get-inherited-property and map-in in Open Firmware 1.0.5.
