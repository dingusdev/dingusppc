# DingusPPC User Manual

## Implemented Features

* Interpreter (with 601, FPU, and MMU support)
* IDE and SCSI
* Floppy disk image reading (Raw, Disk Copy 4.2, WOZ v1 and v2)
* ADB mouse, keyboard, and AppleJack (Pippin) controller
* Some audio support
* Basic video output support (i.e. ATI Rage, Control, Platinum)

## Known Working OSes

* Disk Tools (7.1.2 - 8.5)
* Mac OS 7.1.2 - 9.2.2 (from CD or Hard Disk)
* OpenDarwin 6.6.2

## Disk Initialization

To initialize a disk, you'll first need to boot Disk Tools from a floppy disk image along with a blank hard disk image you want to initialize.

Once booted, start up the Hard Disk Setup application and initialize the drive listed. It may be listed as not mounted.

Once the disk has been initialized, you will need to reboot the machine with the appropriate installation media, such as a CD-ROM image.

Make sure the disk is appropriately sized for the OS you want to install and follow the instructions from the installer carefully.

You may also want to use a third-party program like BlueSCSI to convert a raw HFS image from an emulator like SheepShaver.

## Windows

DingusPPC uses two windows when booted up; a command line window and a monitor window to display the machine.

## Commands

DingusPPC is operated using the command line interface. As such, we will list the commands as required. These commands are separated by spaces.

```
-r, --realtime
```

Run the emulator in runtime (using the interpreter).

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
--rambank1_size X
--rambank2_size X
--rambank3_size X
--rambank4_size X
```

Set the RAM sizes to use, with X being an integer of a power of 2 up to 1024 (depending on the emulated machine).

```
--fdd_img TEXT:FILE
```

Set the floppy disk image

```
--fdd_wr_prot=1
```

Set the floppy to read-only

```
--hdd_img TEXT:FILE
```

Set the hard disk image

```
--cdr_img TEXT:FILE
```

Set the CD ROM image

```
--cpu
```

Change which version of the PowerPC CPU to use

```
--emmo
```

Access the factory tests

```
--serial_backend=stdio
--serial_backend=socket
```

Change where the output of Open Firmware is directed to, either to the command line (with stdio) or a Unix socket (unavailable in Windows builds). Open Firmware 1.x outputs here by default.

```
--adb_devices TEXT
```

Set the ADB devices to attach, comma-separated.

### Command Line Examples

```
dingusppc -b bootrom-6100.bin --rambank1_size 64 --rambank2_size 64 --hdd_img "System_712.dsk"
```

The user has specified their own ROM file, which is for a Power Macintosh 6100 and has also set up two separate RAM banks to use 64 MB each. Note that if a second RAM bank is to be specified for the 6100, it should be the same size as the first RAM bank. With only a hard disk specified, the machine will immediately boot to the OS on the hard disk.

```
dingusppc -b "Power_Mac_G3_Beige.ROM" -r --rambank1_size 128 --fdd_img "DiskTools_8.5.img"
```

Here, the user has attached a floppy disk image. They've chosen to boot it from a G3 and the first RAM bank is set to 128 MB.

```
dingusppc -b "Power_Mac_G3_Beige.ROM" -d --rambank1_size 64 --rambank2_size 64 --cdr_img "OpenDarwin_662.cdr"
```

The debugger will be turned on here, due to the presence of `-d`. The CD ROM image will be loaded in.

## Accessing Open Firmware

After booting from a PCI Power Mac ROM without any disk images, enter the debugger and change the NVRAM property `auto-boot?` to false. Exit out of the emulator and boot it back up to access it.

## Supported machines

The machines that currently work the best are the Power Mac 6100, the Power Mac 7500, and the Power Mac G3.

Early implementations of the iMac G3, Power Mac G3 Blue and White, and Apple Pippin are also present.

## Debugger

The debugger can be used to show what code is currently being executed, the contents of memory as hex or 68K assembly or PowerPC assembly, NVRAM variables, memory regions, and CPU registers. It can also be used to change memory, registers, and nvram variables. It can step through instructions one at a time or many instructions at once.

## Quirks
### Mouse Grabbing

While the emulator display window is in focus, press Control-G to hide the host mouse cursor to control the guest mouse cursor directly. Press Control-G to show the host mouse cursor again and control the guest mouse cursor only when the host mouse cursor is within the emulator display window.

Press Control-S to toggle the scaling method of the emulator display window. The methods are nearest (best used when the scale factor is an integer value) and linear (or smooth mode which is not as sharp but helps display quality when the scale factor is not an integer value).

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
