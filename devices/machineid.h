#ifndef MACHINE_ID_H
#define MACHINE_ID_H

#include <cinttypes>
#include "mmiodevice.h"

/**
    @file Contains definitions for PowerMacintosh machine ID registers.

    The machine ID register is a memory-based register containing hardcoded
    values the system software can read to identify machine/board it's running on.

    Register location and value meaning are board-dependent.
 */

/**
    The machine ID for the Gossamer board is accesible at 0xFF000004 (phys).
    It contains a 16-bit value revealing machine's capabilities like bus speed,
    ROM speed, I/O configuration etc.
    Because the meaning of these bits is poorly documented, the code below
    simply return raw values obtained from real hardware.
 */
class GossamerID : public MMIODevice {
public:
    GossamerID(const uint16_t id) { this->id = id, this->name = "Machine-id"; };
    ~GossamerID() = default;

    uint32_t read(uint32_t offset, int size) {
         return ((!offset && size == 2) ? this->id : 0); };

    void write(uint32_t offset, uint32_t value, int size) {}; /* not writable */

private:
    uint16_t id;
};

#endif /* MACHINE_ID_H */
