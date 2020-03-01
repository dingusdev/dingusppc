The PowerPC is the main processor behind Power Macs.

# General Notes

All instructions are 32 bits wide, regardless of whether the PowerPC is in 32-bit or 64-bit mode.

Code execution generally begins at 0xFFF00100, which the reset exception vector.

# BATs

The 601 BATs are emulated by the OpenFirmware.

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
| Machine State Register            | 1                      |                                                       |
