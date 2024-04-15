# RAM Expansion in Power Macintosh 6100

Power Macintosh 6100 comes with two RAM slots accepting 72-pin SIMMs.

Because a 72-pin SIMM only has 32 data lines but the PowerPC CPU bus
is 64-bit wide two SIMMs with the same capacity installed in both slots are
required.

Moreover, each 72-pin SIMM can have one or two banks of memory.
RAM banks are not to be confused with memory slots on the motherboard!
A single bank SIMM will have only one side populated with RAM chips
while a two banks SIMM will contain chips on both sides.

A 72-pin SIMM has 12 address lines so the maximum amount of RAM one bank
can hold is calculated as follows:

```
2^row_bits * 2^column_bits * (bus_width/8) = 2^12 * 2^12 * 8 = 128 MB
```

Considering two banks of memory the maximum amount of RAM one can plug into
a Power Macintosh 6100 is 256 MB.

Both parity (36bit) and non-parity (32bit) SIMMs should work fine in this machine.

Always use two SIMMs with the same configuration and capacity to extend the RAM in
your Power Macintosh 6100!

## Supported SIMM sizes

Power Macintosh 6100 supports the following SIMM sizes and configurations:

| Bank size | Number of rows | Number of columns | Total RAM |
|:---------:|:--------------:|:-----------------:|:---------:|
| 1 MB      | 9              | 9                 | 2 MB      |
| 2 MB      | 10             | 9                 | 4 MB      |
| 4 MB      | 10             | 10                | 8 MB      |
| 8 MB      | 11             | 10                | 16 MB     |
| 16 MB     | 11             | 11                | 32 MB     |
| 16 MB     | 12             | 10                | 32 MB     |
| 32 MB     | 12             | 11                | 64 MB     |
| 64 MB     | 12             | 12                | 128 MB    |


## RAM sizing

Although the 72-pin SIMM provides four identification aka **presence detection**
pins the Power Macintosh 6100 motherboard actually leaves them unconnected.

To determine the size of installed RAM the low-level software in the computer's
ROM runs a specialized procedure called **RAM sizing**.

Its algorithm proceeds as follows:

1. set physical bank size to 128 MB so the memory controller will drive all
row and column bits
2. write a 64-bit test pattern to the last QWORD of each physical bank
3. if the test pattern can be read back assume some memory is present; proceed
with step 5
4. decrement memory address and return to step 2 or exit if no memory was found
5. search upwards for the first occurence of the test pattern written in step 2
starting with the first address of the particular bank; the first location the
test pattern can be found at indicates the size of the installed memory.

To fully understand the above algorithm let's work through the following example:

Step 2 writes a QWORD at offset `0x07FFFFF8` that corresponds to 128 MB - 8.
The Highspeed Memory Controller (HMC) decodes that offset as follows:

```
aaaaa | rrrrrrrrrrrr | cccccccccccc | lll
------------------------------------------
00000 | 111111111111 | 111111111111 | 000
```

where `a` indicates the bank starting address, `r` corresponds to the row bits,
`c` - the column bits and `l` tells which bytes in a QWORD will be affected.

When address `0x07FFFFF8` is accessed, a 64 MB SIMM will decode all row and column
bits so the destination memory cells will be at `row=0xFFF,col=0xFFF`. On the
contrary, a 1 MB SIMM will only decode 9 row and 9 column bits so the destination
cells will be at `row=0x1FF,col=0x1FF`. A test pattern written at offsets
`0x07FFFFF8` can be read back at any address higher than 1MB causing all 9 row
and 9 column bits to be set. Thus finding the first occurence of the test pattern
as described in setp 5 will give the actual size of the installed SIMM.

That's how a 64 MB of RAM is sized:

```
Write test pattern to  0x07FFFFF8 --> Row bits: 0xFFF, Column bits: 0xFFF
Read test pattern from 0x07DFFFF8 --> Row bits: 0xFBF, Column bits: 0xFFF
```

The first address the test pattern can be read back at is `0x07FFFFF8`
so the amount of installed RAM is 128 MB (64 MB + 64 MB).

That's how a 8 MB of RAM is sized:

```
Write test pattern to  0x07FFFFF8 --> Row bits: 0x3FF, Column bits: 0x3FF
...
Read test pattern from 0x007ffff8 --> Row bits: 0x3FF, Column bits: 0x3FF
```

The first address the test pattern can be read back at is `0x007FFFF8`
so the amount of installed RAM is 8 MB (4MB + 4MB).

Now the last example for 2 MB RAM:

```
Write test pattern to  0x07FFFFF8 --> Row bits: 0x1FF, Column bits: 0x1FF
Read test pattern from 0x07DFFFF8 --> Row bits: 0x1FF, Column bits: 0x1FF
...
Read test pattern from 0x001FFFF8 --> Row bits: 0x1FF, Column bits: 0x1FF
```

The first address the test pattern can be read back at is `0x001FFFF8`
so the amount of installed RAM is 2 MB (1 MB + 1 MB).
