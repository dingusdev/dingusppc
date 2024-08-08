The PowerPC is the main processor behind Power Macs. Currently, DingusPPC only implements the 32-bit variant.

# General Notes

All instructions are 32 bits wide, regardless of whether the PowerPC is in 32-bit or 64-bit mode.

Code execution generally begins at 0xFFF00100, which the reset exception vector.

# BATs

The 601 BATs are emulated by the Open Firmware.

# TLBs

Up to 128 instruction entries and 128 data entries can be stored at a time.

# Processor Revisions

| Model         | PVR Number  | Notable Aspects             |
| :------------ | :---------- | :-------------------------- |
| 601           | 0x00010001  | Supports POWER instructions |
| 603           | 0x00030001  | Software-controlled TLBs    |
| 604           | 0x00040103  | Ability for Multiprocessing |
| 603E          | 0x00060101  |                             |
| 750 (G3)      | 0x00080200  | Built-in L2 data cache      |
| 7400 (G4)     | 0x000C0101  | AltiVec/VMX support added   |

# Registers

| Register Type                     | Number                 | Purpose                                               |
| :-------------------------------- | :--------------------- | :---------------------------------------------------- |
| General Purpose (GPR)             | 32                     | Calculate, Store, and Load 32-bit fixed-point numbers |
| Floating Point (FPR)              | 32                     | Calculate, Store, and Load 32-bit or 64-bit floating-point numbers |
| Special Purpose (SPR)             | Up to 1024 (in theory) | Store and load specialized 32-bit fixed-point numbers |
| Segment (SR)                      | 16                     | Calculate, Store, and Load 32-bit fixed-point numbers |
| Time Base Facility (TBR)          | 2                      | Calculate, Store, and Load 32-bit fixed-point numbers |
| Condition Register                | 1                      | Stores conditions based on the results of fixed-point operations |
| Floating Point Condition Register | 1                      | Stores conditions based on the results of floating-point operations |
| Vector Status and Control Register (VSCR) | 1              | Stores conditions based on the results of vector operations |
| Machine State Register            | 1                      |                                                       |


# Special Registers

| Register Name                     | Register Number      | Purpose                                               |
| :-------------------------------- | :------------------- | :---------------------------------------------------- |
| Multiply Quotient Register (MQ)   | 0                    | (601 only)                                            |
| Integer Exception (XER)           | 1                    |                                                       |
| RTC Upper Register (RTCU)         | 4                    | (601 only)                                            |
| RTC Lower Register (RTCL)         | 5                    | (601 only)                                            |
| Link Register (LR)                | 8                    |                                                       |
| Counter Quotient Register (CTR)   | 9                    |                                                       |
| Vector Save/Restore               | 256                  | (G4+)                                                 |
| Time Base Lower (TBL)             | 268                  | (603+)                                                |
| Time Base Upper (TBU)             | 269                  | (603+)                                                |
| External Access (EAR)             | 282                  |                                                       |
| Processor Version (PVR)           | 287                  |                                                       |
| Hardware Implementation 0 (HID0)  | 1008                 |                                                       |
| Hardware Implementation 1 (HID1)  | 1009                 |                                                       |

# HID 0

| Model         | Bits Enabled        |
| :------------ | :------------------ |
| 601           | (NOT PRESENT)       |
| 603           | NHR, DOZE/NAP/SLEEP |
| 604           | NHR                 |
| 603E          | NHR, DOZE/NAP/SLEEP |
| 603EV         | NHR, DOZE/NAP/SLEEP |
| 604E          | NHR                 |
| 750 (G3)      | NHR, DOZE/NAP/SLEEP |

# Eccentricities

* The HW Init routine used in the ROMs uses the DEC (decrement; SPR 22) register to measure CPU speed. With a PowerPC 601, the DEC register operates on the same frequency as RTC - 7.8125 MHz but uses only 25 most significant bits. In other words, it decrements by 128 at 1/7.8125 MHz.

* Apple's memcpy routine uses double floating-point registers rather than general purpose registers to load and store 2 32-bit values at once. As the PowerPC usually operates on at least a 64-bit bus and floating-point processing comes with the processors by default, this saves some instructions and results in slightly faster speeds.

* As the PowerPC does not have an instruction to load an immediate 32-bit value, it's common to see a lis/ori coding pattern.

* The 603 relies on the instructions tlbld and tlbli to assist in TLB reloading.

* To accommodate for early programs compiled on PowerPC 601 Macs, the classic Mac OS has to emulate the POWER instructions that were removed from later processors.

# References

http://www.ibmfiles.com/ibmfiles/powerpc/itso_powerpc_inside_view.pdf
