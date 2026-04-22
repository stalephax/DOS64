#pragma once
#include "../fs/FAT32.h"
#include "../memory.h"
#include "../standart.h"

// Taille du RAM disk — 1 Mo par défaut
static const unsigned int RAMDISK_SIZE    = 1024 * 1024;  // 1 Mo
static const unsigned int RAMDISK_SECTORS = RAMDISK_SIZE / 512;

// ============================================================
// Driver "disque" qui lit/écrit dans la RAM
// Même interface qu'ATADriver pour être compatible avec FAT32
// ============================================================
class RamDisk {
    unsigned char* data;        // buffer en mémoire
    unsigned int   size;        // taille en octets
    bool           present = false;

public:
    bool init(unsigned char* buf, unsigned int sz) {
        data    = buf;
        size    = sz;
        present = true;
        return true;
    }

    // Même interface qu'ATADriver
    bool read(unsigned int lba, unsigned char count, void* buffer) {
        if (!present) return false;
        unsigned int offset = lba * 512;
        if (offset + count * 512 > size) return false;
        memcpy(buffer, data + offset, count * 512);
        return true;
    }

    bool write(unsigned int lba, unsigned char count, const void* buffer) {
        if (!present) return false;
        unsigned int offset = lba * 512;
        if (offset + count * 512 > size) return false;
        memcpy(data + offset, buffer, count * 512);
        return true;
    }

    bool is_present() { return present; }
    const char* get_model() { return "DOS64 RAM Disk"; }
    unsigned int get_size() { return size; }
};

// ============================================================
// Formater le RAM disk en FAT32
// ============================================================
class RamDiskFormatter {
public:
    static bool format(RamDisk* disk, const char* label = "RAMDISK   ") {
        unsigned char buf[512];

        // Zéroer tout le disque
        for (int i = 0; i < 512; i++) buf[i] = 0;
        for (unsigned int s = 0; s < RAMDISK_SECTORS; s++)
            disk->write(s, 1, buf);

        // =====================
        // Boot Record (BPB)
        // =====================
        FAT32_BPB* bpb = (FAT32_BPB*)buf;

        // Jump + NOP
        buf[0] = 0xEB; buf[1] = 0x58; buf[2] = 0x90;

        // OEM
        buf[3]='D'; buf[4]='O'; buf[5]='S'; buf[6]='6';
        buf[7]='4'; buf[8]=' '; buf[9]=' '; buf[10]=' ';

        bpb->bytes_per_sector    = 512;
        bpb->sectors_per_cluster = 1;       // 1 secteur par cluster = 512 octets
        bpb->reserved_sectors    = 4;       // 4 secteurs réservés
        bpb->fat_count           = 2;       // 2 FATs
        bpb->root_entry_count    = 0;       // FAT32 = 0
        bpb->total_sectors_16    = 0;
        bpb->media_type          = 0xF8;
        bpb->fat_size_16         = 0;
        bpb->sectors_per_track   = 63;
        bpb->head_count          = 255;
        bpb->hidden_sectors      = 0;
        bpb->total_sectors_32    = RAMDISK_SECTORS;

        // FAT32 spécifique
        // FAT size = ceil(total_clusters * 4 / 512)
        unsigned int data_sectors  = RAMDISK_SECTORS - 4;
        unsigned int total_clusters = data_sectors;  // 1 secteur/cluster
        unsigned int fat_size      = (total_clusters * 4 + 511) / 512;

        bpb->fat_size_32    = fat_size;
        bpb->ext_flags      = 0;
        bpb->fs_version     = 0;
        bpb->root_cluster   = 2;      // cluster racine = 2 (toujours)
        bpb->fs_info        = 1;
        bpb->backup_boot    = 6;
        bpb->drive_number   = 0x80;
        bpb->boot_signature = 0x29;
        bpb->volume_id      = 0x12345678;

        // Label volume (11 chars padded)
        for (int i = 0; i < 11; i++)
            bpb->volume_label[i] = label[i] ? label[i] : ' ';

        // FS Type
        buf[82]='F'; buf[83]='A'; buf[84]='T';
        buf[85]='3'; buf[86]='2'; buf[87]=' ';
        buf[88]=' '; buf[89]=' ';

        // Signature boot sector
        buf[510] = 0x55; buf[511] = 0xAA;

        disk->write(0, 1, buf);

        // =====================
        // FAT1 et FAT2
        // =====================
        unsigned int fat1_start = bpb->reserved_sectors;
        unsigned int fat2_start = fat1_start + fat_size;

        for (int i = 0; i < 512; i++) buf[i] = 0;
        disk->write(fat1_start, 1, buf);
        disk->write(fat2_start, 1, buf);

        // Entrées spéciales FAT[0] et FAT[1]
        unsigned int* fat = (unsigned int*)buf;
        fat[0] = 0x0FFFFFF8;  // media type
        fat[1] = 0x0FFFFFFF;  // end of chain
        fat[2] = 0x0FFFFFFF;  // cluster 2 = root dir (fin de chaîne)
        disk->write(fat1_start, 1, buf);
        disk->write(fat2_start, 1, buf);

        // =====================
        // Répertoire racine
        // =====================
        for (int i = 0; i < 512; i++) buf[i] = 0;
        unsigned int data_start = fat1_start + fat_size * 2;
        unsigned int root_lba   = data_start;  // cluster 2
        disk->write(root_lba, 1, buf);

        return true;
    }
};