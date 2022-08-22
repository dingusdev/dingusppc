# Open Firmware in Power Macintosh

*compiled from various sources by Max Poliakovski.*

[Open Firmware](https://en.wikipedia.org/wiki/Open_Firmware) is a platform-independent
boot firmware architecture covered by an IEEE standard.

All PowerMacintosh computers run Open Firmware except the very first generation
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
controlling the boot enviroment as by developers for developing and debugging device
drivers.

This document focuses on various aspects of Apple's Open Firmware implementation
as found in various PowerMacintosh models.

## Open Firmware Versions

### Old World Macs

| ROM dump/Machine                     | BootROM codename | OF version    | OF image offset |
|:------------------------------------:|:----------------:|:-------------:|:---------------:|
| Power Macintosh 7300/7600/8600/9600  | TNT              | 1.0.5         | 0x330000        |
| Bandai Pippin                        | Pip              | 1.0.5         | 0x330000        |
| Power Macintosh,Performa 6400        | Alchemy          | 2.0           | 0x330000        |
| Power Macintosh G3 desktop           | Gossamer         | 2.0f1         | 0x320000        |
| PowerBook G3 Wallstreet v2           | GRX              | 2.0.1         | 0x330000        |
| Power Macintosh 4400/7220            | Zanzibar         | 2.0.2         | 0x330000        |
| Power Macintosh 6500                 | Gazelle          | 2.0.3         | 0x330000        |
| Power Macintosh G3 v3                | Gossamer         | 2.4           | 0x320000        |

### NewWorld Macs

*TBD*

## Open Firmware image

### Old World Macs

Open Firmware in OldWorld Macs is stored in the monolithic 4MB ROM. Its hibernated
image is located at offset `0x320000` or `0x330000` from beginning of the ROM.
That corresponds to the physical address `0xFFF20000` or `0xFFF30000`, respectively.

The size of the Open Firmware image varies from 98KB (v1.0.5) to 172KB (v2.4).

Apple's Open Firmware image has the following structure:

| Section type       | Architecture | Relative Size (v1.0.5) | Relative Size (v2.4) |
|:------------------:|:------------:|:----------------------:|:--------------------:|
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
Right after the COFF section header, a vital OF kernel data structure called `StartVec`
is located. It contains among others several important offsets into the OF image
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


### Open Firmware internals

Apple's Open Firmware contains a small kernel implemented in the native PowerPC code. This kernel performs the following actions:

* set up memory translation for OF execution
* relocate itself from ROM to RAM
* initialize Forth runtime environment
* recompile OF main image from FCode to native PPC code
* pass control to recompiled OF that starts building the device tree
* process low-level exceptions

## Open Firmware and the Macintosh boot process

### Old World Macs

1. In response to power coming on, HWInit code in the Power Macintosh ROM performs initialization of the memory controller and the basic I/O facilities as well as some self-testing. After that, the startup chime is played.
2. HWInit passes control to Open Firmware kernel that prepares OF execution from RAM. OF builds the **device tree** - a platform-independent description of the attached HW.
3. OF returns control to HWInit that initializes several low-level data structures required by the Nanokernel.
4. HWInit passes control to the Nanokernel that initializes the native execution enviroment and the 68k emulator.
5. 68k emulator executes the start-up code in the Macintosh ROM that initializes various managers.
6. The device tree generated by the Open Firmware in step 2 is imported by the Expansion Bus Manager initialization code and stored in the **NameRegistry**.
7. An operating system is located and loaded.

### New World Macs

*TBD*

## Open Firmware bugs

Apple OF is known to contain numerous bugs. The following table lists some recently discrovered bugs, not mentioned elsewhere.

| OF version affected | Bug description |
|:-------------------:|-----------------|
| 2.0f1, 2.4          | A numerical overflow in `um/mod` used by  `get-usecs-60x` causes the OF console to become unresponsive after approx. 71 minutes. You have to restart your computer once the bug is triggered. |
