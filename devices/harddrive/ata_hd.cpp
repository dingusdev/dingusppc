#include "ata_hd.h"
#include <fstream>
#include <limits>
#include <stdio.h>

#define sector_size 512

using namespace std;

HardDrive::HardDrive(std::string filename) {
    this->hdd_img;

    // Taken from:
    // https://stackoverflow.com/questions/22984956/tellg-function-give-wrong-size-of-file/22986486
    this->hdd_img.ignore(std::numeric_limits<std::streamsize>::max());
    this->img_size = this->hdd_img.gcount();
    this->hdd_img.clear();    //  Since ignore will have set eof.
    this->hdd_img.seekg(0, std::ios_base::beg);
}
