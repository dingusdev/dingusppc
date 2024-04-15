# DingusPPC User Manual

## Commands

DingusPPC is operated using the command line interface. As such, we will list the commands as required.

```
-r, --realtime
```

Run the emulator in runtime.

```
-d, --debugger
```

Enter the interactive debugger. The user may also enter the debugger at any point by pressing Control and C.

```
list machines
```

Shows the currently implemented machines within DingusPPC.

```
list properties
```

Shows the configurable properties, such as the selected disc image and the ram bank sizes.

## Supported machines

The machines that currently work the best are the Power Mac 6100, the Power Mac 7500, and the Power Mac G3.

Early implementations for the Power Mac G3 Blue and White and the Apple Pippin are also present.

## Quirks

### CD ROM Images

Currently, ISO images are supported. However, support is not yet implemented for multi-mode CD images.

### Hard Disks

Because Sheepshaver, Basilisk II, and Mini vMac operate on raw disks, it is required to a program such as BlueSCSI to make their hard disk images work in an emulator like DingusPPC.