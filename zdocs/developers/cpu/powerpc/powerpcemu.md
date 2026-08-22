# PowerPC Emulation

Implementation notes for the PowerPC CPU emulator in DingusPPC.

## Time and CPU Timing

DingusPPC normally advances virtual time as it interprets instructions rather than directly from the host's wall clock. Each machine supplies its bus, core, and timebase frequencies. The bus/core ratio determines supported HID1 PLL values, while the timebase frequency controls the guest TBR and decrementer independently of instruction timing.

The `--cpu-timing` option controls how much virtual time each interpreted instruction consumes. The default `fixed` mode uses a fixed 16 ns per instruction for every machine. `per-machine` mode derives the period from the configured core frequency. Both modes use 60.4 fixed-point nanoseconds internally, and neither changes the machine's configured clocks or guest-visible HID1 value.

The interpreter currently assigns the same period to every instruction; it does not model pipelines, caches, or instruction-specific cycle counts. Therefore, `per-machine` is not cycle-accurate, and a host that cannot execute instructions at the selected rate will run guest time more slowly than wall time. Enabling the separate `g_realtime` flag follows the host clock instead, which can expose guest timeouts when the interpreter cannot keep up.
