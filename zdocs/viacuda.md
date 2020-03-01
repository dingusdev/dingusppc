Using a combination of a 6522 along with some integrated circuits, the VIA Cuda is a microcontroller that heavily controls system operations. It's largely similar to Egret (used in many 68k Macs), but removes some commands.

The usual offset for a VIA Cuda is IOBase (ex.: 0xF3000000 for Power Mac G3 Beige) + 0x16000. The registers are spaced out by 0x200 bytes on the Heathrow.

# Usage

The VIA Cuda is emulated in all Power Macs through an interrupt controller. Early Power Macs also used the Parameter RAM.

Additionally, nodes are included for ADB peripherals, Parameter RAM, Real-Time Clocks, and Power Management.

It can be run either synchronously or asynchronously.

However, the CUDA is also slower than the CPU, thus causing a delay that the OS expects.

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
