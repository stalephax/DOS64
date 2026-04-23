// kernel/fs/diskif.h — interface disque commune
#pragma once

struct DiskDrive {
    virtual bool read(unsigned int lba, unsigned char count, void* buf)       = 0;
    virtual bool write(unsigned int lba, unsigned char count, const void* buf) = 0;
    virtual bool is_present() = 0;
};