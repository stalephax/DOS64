// kernel/fs/diskif.h — interface disque commune
#pragma once
#include "../drivers/ata.h"
#include "FAT32.h" 
#include "../drivers/ramdrv.h"
struct DiskDrive {
    virtual bool read(unsigned int lba, unsigned char count, void* buf)       = 0;
    virtual bool write(unsigned int lba, unsigned char count, const void* buf) = 0;
    virtual bool is_present() = 0;
};

// Adapteur pour ATADriver
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

// Adapteur pour RamDisk
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