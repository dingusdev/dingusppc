# DingusPPC User Manual

## Implemented Features

* Interpreter (with FPU and MMU support)
* IDE and SCSI 
* Floppy disk image reading (Raw, Disk Copy 4.2, WOZ v1 and v2)
* ADB mouse and keyboard emulation
* Some audio support
* Basic video output support (i.e. ATI Rage, Control, Platinum)

## Commands

DingusPPC is operated using the command line interface. As such, we will list the commands as required.

```
-r, --realtime
```

Run the emulator in runtime (using the interpeter).

```
-d, --debugger
```

Enter the interactive debugger. The user may also enter the debugger at any point by pressing Control and C.

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
rambank1_size
rambank2_size
rambank3_size
rambank4_size
```

Set the RAM sizes to use

```
fdd_img
```

Set the floppy disk image

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
serial_backend stdio
serial_backend socket
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

Because Sheepshaver, Basilisk II, and Mini vMac operate on raw disks, it is required to a program such as BlueSCSI to make their hard disk images work in an emulator like DingusPPC. This is because the Mac OS normally requires certain values in the hard disks that these emulators don't normally 

### Currently Unimplemented Features

* JIT compiler
* 3D acceleration support
* Additional ADB and USB peripherals
* Networking