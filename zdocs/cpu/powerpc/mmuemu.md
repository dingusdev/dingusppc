# PowerPC Memory Management Unit Emulation

Emulation of a [memory management unit](https://en.wikipedia.org/wiki/Memory_management_unit)
(MMU) in a full system emulator is considered a hard task. The biggest challenge is to do it fast.

In this article, I'm going to describe a solution for a reasonably fast emulation
of the PowerPC MMU.

This article is based on ideas presented in the paper "Optimizing Memory Emulation
in Full System Emulators" by Xin Tong and Motohiro Kawahito (IBM Research Laboratory).

## PowerPC MMU operation
