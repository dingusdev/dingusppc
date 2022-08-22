The Apple Desktop Bus is a bit-serial peripheral bus, Apple themselves cited a 2-MHz Motorola 68HC11 microcontroller as an example platform to implement the ADB standard with. Its transfer speed is usually around 10.0 kilobits per second, roughly comparable to a PS/2 port at 12.0 kilobits per second.

The device commands are in the form of single byte strings. The first four bits are to signal which of the 16 devices are to be used. The next two bits are for which action to execute (talk, listen, flush, or reset). These are followed by two bits which determine the register to reference (register 0 is usually a communications register, while register 3 is used for device info).

# Commands

| Command Name                | Number |
|:---------------------------:|:-------:|
| DEVCMD_CHANGE_ID_AND_ENABLE | 0x00    |
| DEVCMD_CHANGE_ID            | 0xFD    |
| DEVCMD_CHANGE_ID_AND_ACT    | 0xFE    |
| DEVCMD_SELF_TEST            | 0xFF    |

# Devices

| Device Type       | Example       | Default Address |
|:-----------------:|:-------------:|:---------------:|
| Protection        |               | 0x1             |
| Encoded           | Keyboard      | 0x2             |
| Relative-position | Mouse         | 0x3             |
| Absolute-position | Tablet        | 0x4             |
| Data transfer     | Modem         | 0x5             |
| Other             |               | 0x6             |
| Other             |               | 0x7             |