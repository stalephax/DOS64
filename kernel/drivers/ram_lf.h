#pragma once
#include "vdrive.h"
#include "../drivers/ramdrv.h"

class RamDiskIF : public DiskDrive {
    RamDisk* ram;
public:
    RamDiskIF(RamDisk* r) : ram(r) {}
    bool read(unsigned int lba, unsigned char count, void* buf) override {
        return ram->read(lba, count, buf);
    }
    bool write(unsigned int lba, unsigned char count, const void* buf) override {
        return ram->write(lba, count, buf);
    }
    bool is_present() override { return ram->is_present(); }
};