#pragma once
#include "vdrive.h"
#include "../drivers/ata.h"

class ATADiskIF : public DiskDrive {
    ATADriver* ata;
public:
    ATADiskIF(ATADriver* a) : ata(a) {}
    bool read(unsigned int lba, unsigned char count, void* buf) override {
        return ata->read(lba, count, buf);
    }
    bool write(unsigned int lba, unsigned char count, const void* buf) override {
        return ata->write(lba, count, buf);
    }
    bool is_present() override { return ata->is_present(); }
};