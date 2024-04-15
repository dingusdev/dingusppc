Using a combination of a 6522 along with some integrated circuits, the VIA Cuda is a microcontroller that heavily controls system operations. It's largely similar to Egret (used in many 68k Macs), but removes some commands.

The usual offset for a VIA Cuda is IOBase (ex.: 0xF3000000 for Power Mac G3 Beige) + 0x16000. The registers are spaced out by 0x200 bytes on the Heathrow.

# Usage

The VIA Cuda is emulated in all Power Macs through an interrupt controller. Early Power Macs also used the Parameter RAM.

Additionally, nodes are included for ADB peripherals, Parameter RAM, Real-Time Clocks, and Power Management.

It can be run either synchronously or asynchronously.

However, the CUDA is also slower than the CPU, thus causing a delay that the OS expects.

# Registers

Within the emulated CUDA, these registers are spaced apart by 0x200 bytes. Apple themselves recommended avoiding the usage of Handshake Data A.

| Register Name             |Abbreviation| Offset |
|:-------------------------:|:----------:|:------:|
| Data B                    | ORB        | 0x0    |
| Handshake Data A          | ORA(H)     | 0x1    |
| Data Direction B          | DIRB       | 0x2    |
| Data Direction A          | DIRA       | 0x3    |
| Timer 1 Counter Low       | T1C        | 0x4    |
| Timer 1 Counter High      | T1CH       | 0x5    |
| Timer 1 Latch Low         | T1L        | 0x6    |
| Timer 1 Latch High        | T1LH       | 0x7    |
| Timer 2 Counter Low       | T2C        | 0x8    |
| Timer 2 Counter High      | T2CH       | 0x9    |
| Shift                     | SR         | 0xA    |
| Auxiliary Control         | ACR        | 0xB    |
| Peripheral Control        | PCR        | 0xC    |
| Interrupt Flag            | IFE        | 0xD    |
| Interrupt Enable          | IER        | 0xE    |
| Data A                    | ORA        | 0xF    |

## Data A

| Register Bit     | Bit Mask |
|:----------------:|:--------:|
| Drive Select     | 0x10     |
| Disk Head Select | 0x20     |

## Auxiliary Control

| Register Bit                      | Bit Mask |
|:---------------------------------:|:--------:|
| Port A Latch                      | 0x1      |
| Port B Latch                      | 0x2      |
| Timer 2, control                  | 0x20     |
| Timer 1, continuous counting      | 0x40     |
| Timer 1, drives PB7               | 0x80     |

## Interrupt Enable

| Register Bit   | Bit Mask |
|:--------------:|:--------:|
| CA2            | 0x1      |
| CA1            | 0x2      |
| Shift Register | 0x2      |
| CB2            | 0x8      |
| CB1            | 0x10     |
| Timer 2        | 0x20     |
| Timer 1        | 0x40     |
| Set interrupt  | 0x80     |

# Packet Types

| Hex           | Packet Type            |
|:-------------:|:----------------------:|
| 0x00          | ADB                    |
| 0x01          | Pseudo                 |
| 0x02          | Error                  |
| 0x03          | Timer                  |
| 0x04          | Power                  |
| 0x05          | Mac II                 |


# Device Commands

| Hex           | Device Command         |
|:-------------:|:----------------------:|
| 0x00          | Reset Bus              |
| 0x01          | Flush ADB              |
| 0x08          | Write ADB              |
| 0x0C          | Read ADB               |

# Pseudo Commands

| Hex           | Command Name           |
|:-------------:|:----------------------:|
| 0x00          | WARM_START             |
| 0x01          | START_STOP_AUTO_POLL   |
| 0x02          | GET_6805_ADDRESS       |
| 0x03          | GET_REAL_TIME          |
| 0x07          | GET_PRAM               |
| 0x08          | SET_6805_ADDRESS       |
| 0x09          | SET_REAL_TIME          |
| 0x0A          | POWER_DOWN             |
| 0x0B          | SET_POWER_UPTIME       |
| 0x0C          | SET_PRAM               |
| 0x0D          | MONO_STABLE_RESET      |
| 0x0E          | SEND_DFAC              |
| 0x10          | BATTERY_SWAP_SENSE     |
| 0x11          | RESTART_SYSTEM         |
| 0x12          | SET_IPL_LEVEL          |
| 0x13          | FILE_SERVER_FLAG       |
| 0x14          | SET_AUTO_RATE          |
| 0x16          | GET_AUTO_RATE          |
| 0x19          | SET_DEVICE_LIST        |
| 0x1A          | GET_DEVICE_LIST        |
| 0x1B          | SET_ONE_SECOND_MODE    |
| 0x21          | SET_POWER_MESSAGES     |
| 0x22          | GET_SET_IIC            |
| 0x23          | ENABLE_DISABLE_WAKEUP  |
| 0x24          | TIMER_TICKLE           |
| 0X25          | COMBINED_FORMAT_IIC    |
| 0x26          | (UNDOCUMENTED!)        |
