#ifndef PMAC_6100_HW_H
#define PMAC_6100_HW_H

#include <cinttypes>
#include "mmiodevice.h"

/** Some Power macintosh IDs stored in the CPUID register */
enum PMACID {
    PM6100 = 0x3010, /* PowerMac 6100 */
    PM7100 = 0x3012, /* PowerMac 7100 */
    PM8100 = 0x3013, /* PowerMac 8100 */
};


/** ======== High-speed memory controller (HMC) ======== */
class HMC : public MMIODevice {
public:
    HMC();
    ~HMC() = default;
    uint32_t read(uint32_t offset, int size);
    void write(uint32_t offset, uint32_t value, int size);

private:
    int      bit_pos;
    uint64_t config_reg;
};


/** CPUID is a read-only register containing machine identification (see PMACID) */
class CPUID : public MMIODevice {
public:
    CPUID(const uint32_t id) {
        this->cpuid = (0xA55A << 16) | (id & 0xFFFF); };
    ~CPUID() = default;
    uint32_t read(uint32_t offset, int size) {
             return (cpuid >> (3 - (offset & 3)) * 8) & 0xFF; };
    void write(uint32_t offset, uint32_t value, int size) {}; /* not writable */

private:
    uint32_t cpuid;
};

void pmac6100_init(void);

#endif /* PMAC_6100_HW_H */
