# DingusPPC User Manual

## Implemented Features

* Interpreter (with 601, FPU, and MMU support)
* IDE and SCSI 
* Floppy disk image reading (Raw, Disk Copy 4.2, WOZ v1 and v2)
* ADB mouse and keyboard emulation
* Some audio support
* Basic video output support (i.e. ATI Rage, Control, Platinum)

## Known Working OSes

* Disk Tools (7.1.2 - 8.5) 
* Mac OS 7.1.2 - 9.2.2 (from CD)
* Mac OS 7.5.3 - 9.2.2 (from Hard Disk)
* OpenDarwin 6.6.2

## Windows

DingusPPC uses two windows when booted up; a command line window and a monitor window to display the machine.

## Commands

DingusPPC is operated using the command line interface. As such, we will list the commands as required. These commands are separated by spaces.

```
-r, --realtime
```

Run the emulator in runtime (using the interpeter).

```
-d, --debugger
```

Enter the interactive debugger. The user may also enter the debugger at any point by pressing Control and C, when the command line window is selected.

```
-b, --bootrom TEXT:FILE
```

Specifies the Boot ROM path (optional; looks for bootrom.bin by default)

```
-m, --machine TEXT
```

Specify machine ID (optional; will attempt to determine machine ID from the boot rom otherwise)

```
list machines
```

Shows the currently implemented machines within DingusPPC.

```
list properties
```

Shows the configurable properties, such as the selected disc image and the ram bank sizes.

### Properties

```
rambank1_size X
rambank2_size X
rambank3_size X
rambank4_size X
```

Set the RAM sizes to use, with X being an integer of a power of 2 up to 256.

```
fdd_img
```

Set the floppy disk image

```
fdd_wr_prot=1
```

Set the floppy to read-only

```
hdd_img
```

Set the hard disk image

```
cdr_img
```

Set the CD ROM image

```
cpu
```

Change which version of the PowerPC CPU to use

```
emmo
```

Access the factory tests

```
serial_backend=stdio
serial_backend=socket
```

Change where the output of OpenFirmware is directed to, either to the command line (with stdio) or a Unix socket (unavailable in Windows builds). OpenFirmware 1.x outputs here by default.

## Accessing OpenFirmware

After booting from a PCI Power Mac ROM without any disk images, enter the debugger and change the NVRAM property `auto-boot?` to false. Quit of the emulator and boot it back up to access it.

## Supported machines

The machines that currently work the best are the Power Mac 6100, the Power Mac 7500, and the Power Mac G3.

Early implementations for the Power Mac G3 Blue and White and the Apple Pippin are also present.

## Debugger

The debugger can be used to not only see what code is being executed at a given moment, but also see what is stored in an NVRAM portion.

## Quirks
### Mouse Grabbing

While the emulator window is in focus, press Control and G to access.

### CD ROM Images

Currently, ISO images are supported. However, support is not yet implemented for multi-mode CD images.

### Hard Disks

Because Sheepshaver, Basilisk II, and Mini vMac operate on raw disks, it is required to a program such as BlueSCSI to make their hard disk images work in an emulator like DingusPPC. This is because the Mac OS normally requires certain values in the hard disks that these emulators don't normally insert into the images. You may also need a third-party utility to create an HFS or HFS+ disk image.

### OS Support

Currently, the Power Mac 6100 cannot boot any OS image containing Mac OS 8.0 or newer.

### Currently Unimplemented Features

* JIT compiler
* AltiVec
* 3D acceleration support
* Additional ADB and USB peripherals
* Networking