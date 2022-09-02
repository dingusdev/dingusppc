
/** @file ATA hard drive support */

#ifndef SCSI_HD_H
#define SCSI_HD_H

#include <cinttypes>
#include <stdio.h>
#include <string>


class ATAHardDrive {
public:
    ATAHardDrive(std::string filename);
}

#endif