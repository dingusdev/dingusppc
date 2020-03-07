OpenFirmware is a standard to defined a firmware's interfaces. It primarily uses the Forth programming language. It's available on most PCI Power Macs and all New World Power Macs by holding Command, Option, O, and F together during bootup.

The bootpath for Old World Macs is usually set to "/AAPL,ROM". New World Macs specify "\\:tbxi" as the boot path instead.

When using an Old World PCI Power Mac, it gets relocated from 0xFF800000 to 0x00400000 during boot-up, then instruction and data translations are enabled.

There is also a section of NVRAM reserved for boot-up code called "NVRAMRC", ran each time the system is booted up.

It gets compiled to native PowerPC code during boot-up.
