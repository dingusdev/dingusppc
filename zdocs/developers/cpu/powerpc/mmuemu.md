# PowerPC Memory Management Unit Emulation

Emulation of a [memory management unit](https://en.wikipedia.org/wiki/Memory_management_unit)
(MMU) in a full system emulator is considered a hard task. The biggest challenge is to do it fast.

In this article, I'm going to present a solution for a reasonably fast emulation
of the PowerPC MMU.

This article is based on ideas presented in the paper "Optimizing Memory Emulation
in Full System Emulators" by Xin Tong and Motohiro Kawahito (IBM Research Laboratory).

## PowerPC MMU operation

The operation of the PowerPC MMU can be described using the following pseudocode:

```
VA is the virtual address of some memory to be accessed
PA is the physical address of some memory translated by the MMU
AT is access type we want to perform

if address translation is enabled:
    PA = block_address_translation(VA)
    if not PA:
        PA = page_address_translation(VA)
else:
    PA = VA

if access_permitted(PA, AT):
    perfom_phys_access(PA)
else:
    generate_mmu_exception(VA, PA, AT)
```

A HW MMU usually performs several operations in a parallel fashion so the final
address translation and memory access only take a few CPU cycles.
The slowest part is the page address translation because it requires accessing
system memory that is usually an order of magnitudes slower than the CPU.
To mitigate this, a PowerPC CPU includes some very fast on-chip memory used for
building various [caches](https://en.wikipedia.org/wiki/CPU_cache) like
instruction/data cache as well as
[translation lookaside buffers (TLB)](https://en.wikipedia.org/wiki/Translation_lookaside_buffer).

## PowerPC MMU emulation

### Issues

An attempt to mimic the HW MMU operation in software will likely have a poor
performance. That's because modern hardware can perform several tasks in parallel.
However, software has to do almost everything serially. Thus, accessing some memory
with address translation enabled can take up to 300 host instructions! Considering
the fact that every 10th instruction is a load and every 15th instruction is a store,
it will be nearly impossible to achieve a performance comparable to that of the
original system.

Off-loading some operations to the MMU of the host CPU for speeding up emulation
isn't feasible because Apple's computers often have hardware being accessed like an
usual memory. Thus, an emulator needs to distinguish between accesses to real memory
(ROM or RAM) from accesses to memory-mapped peripheral devices. The only way to
do that is to maintain special software descriptors for each virtual memory region
and consult them on each memory access.

### Solution

My solution for a reasonable fast MMU emulation employs a software TLB. It's
used for all memory accesses even when address translation is disabled.

The first stage of the SoftTLB uses a
[direct-mapped cache](https://en.wikipedia.org/wiki/Cache_placement_policies#Direct-mapped_cache)
called **primary TLB** in my implementation. That's because this kind of cache
is the fastest one - one lookup requires up to 15 host instructions. Unfortunately,
this cache is not enough to cover all memory accesses due to a high number of
collisions, i.e. when several distinct memory pages are mapped to the same cache
location.

That's why, the so-called **secondary TLB** was introduced. Secondary TLB is a
4-way fully associative cache. A lookup in this cache is slower than a lookup in the
primary TLB. But it's still much faster than performing a full page table walk
requiring hundreds of host instructions.

All translations for memory-mapped I/O go into the secondary TLB because accesses
to such devices tend to be slower than real memory accesses in the real HW anyways.
Moreover, they usually bypass CPU caches (cache-inhibited accesses). But there
are exceptions from this rule, for example, video memory.

When no translation for a virtual address was found in either cache, a full address
translation including the full page table walk is performed. This path is the
slowest one. Fortunately, the probabilty that this path will be taken seems to be
very low.

The complete algorithm looks like that:
```
VA is the virtual address of some memory to be accessed
PA is the physical address of some memory translated by the MMU
AT is access type we want to perform

PA = lookup_primary_tlb(VA)
if VA in the primary TLB:
    perform_memory_access(PA, ART)
else:
    PA = lookup_secondary_tlb(VA)
    if VA not in the secondary TLB:
        PA = perform_full_address_translation(VA)
        refill_secondary_tlb(VA, PA)
        if is_real_memory(PA):
            refill_primary_tlb(VA, PA)
    perform_memory_access(PA, ART)
```
