## Disabling BAT translation

BAT translation can be disabled by invalidating BAT registers. This is somewhat CPU specific.
MPC601 implements its own format for BAT registers that differs from the PowerPC specification.

MPC601-specific lower BAT registers has the "V" bit. If it's cleared, the corresponding BAT pair
is invalid and won't be used for address translation. To invalidate BATs on MPC601, it's enough
to write NULL to lower BAT registers. That's exactly what Power Mac 6100 ROM does:
 ```
li        r0,     0
mtspr     ibat0l, r0
mtspr     ibat1l, r0
mtspr     ibat2l, r0
```

PowerPC CPUs starting with 603 uses the BAT register format described in the PowerPC specification.
The upper BAT registers contain two bits: Vs (supervisor state valid bit) and Vp (problem/user state valid bit).
PowerPC Architecture First Edition from 1993 gives the following code:

```BAT_entry_valid = (Vs & ~MSR_PR) | (Vp & MSR_PR)```

If neither Vs nor Vp is set, the corresponding BAT pair isn't valid and doesn't participate in address translation.
To invalidate BATs on non-601, it's sufficient to set the upper BAT register to 0x00000000.
