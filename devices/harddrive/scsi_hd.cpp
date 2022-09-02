#include "scsi_hd.h"
#include <fstream>
#include <limits>
#include <stdio.h>

#define sector_size 512

using namespace std;

HardDrive::HardDrive(std::string filename) {
    supports_types(HWCompType::SCSI_DEV);

    this->hdd_img.open(filename, ios::out | ios::in | ios::binary);

    // Taken from:
    // https://stackoverflow.com/questions/22984956/tellg-function-give-wrong-size-of-file/22986486
    this->hdd_img.ignore(std::numeric_limits<std::streamsize>::max());
    this->img_size = this->hdd_img.gcount();
    this->hdd_img.clear();    //  Since ignore will have set eof.
    this->hdd_img.seekg(0, std::ios_base::beg);
}

int HardDrive::test_unit_ready(){
    return 0x0;
}

int HardDrive::req_sense(uint8_t alloc_len) {
    if (alloc_len != 252) {
        LOG_F(WARNING, "Inappropriate Allocation Length: %%d", alloc_len);
    }
    return 0x0; //placeholder - no sense
}

int HardDrive::inquiry() {
    return 0x1000000F;
}

int HardDrive::send_diagnostic() {
    return 0x0;
}

int HardDrive::read_capacity_10() {
    return this->img_size;
}

void HardDrive::format() {
}

void HardDrive::read(uint32_t lba, uint16_t transfer_len) {
    uint32_t final_transfer = (transfer_len == 0) ? 256 : transfer_len;
    final_transfer          = transfer_len * sector_size;

    this->hdd_img.seekg(lba * sector_size);
    img_buffer = this->hdd_img.read(memblock, size);
}

void HardDrive::write(uint32_t lba, uint16_t transfer_len) {
    uint32_t final_transfer = (transfer_len == 0) ? 256 : transfer_len;
    final_transfer          = transfer_len * sector_size;

    this->hdd_img.seekg(device_offset, this->hdd_img.beg);
    this->hdd_img.write((char*)&read_buffer, sector_size);

}

void HardDrive::seek(uint32_t lba) {
}


void HardDrive::rewind() {
    this->hdd_img.seekg(0, disk_image.beg);
}
