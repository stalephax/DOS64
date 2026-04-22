#pragma once
#include "../prompt.h"
#include "../drivers/ata.h"
#include "../standart.h"
#include "vdrive.h"
// =============================================
// Structure du Boot Parameter Block (BPB)
// =============================================
struct FAT32_BPB {
    unsigned char  jump[3];           // JMP instruction
    unsigned char  oem[8];            // OEM name
    unsigned short bytes_per_sector;  // Généralement 512
    unsigned char  sectors_per_cluster;
    unsigned short reserved_sectors;  // Secteurs avant la FAT
    unsigned char  fat_count;         // Nombre de FATs (généralement 2)
    unsigned short root_entry_count;  // 0 pour FAT32
    unsigned short total_sectors_16;  // 0 pour FAT32
    unsigned char  media_type;
    unsigned short fat_size_16;       // 0 pour FAT32
    unsigned short sectors_per_track;
    unsigned short head_count;
    unsigned int   hidden_sectors;
    unsigned int   total_sectors_32;

    // FAT32 specific
    unsigned int   fat_size_32;       // Secteurs par FAT
    unsigned short ext_flags;
    unsigned short fs_version;
    unsigned int   root_cluster;      // Premier cluster du root dir
    unsigned short fs_info;
    unsigned short backup_boot;
    unsigned char  reserved[12];
    unsigned char  drive_number;
    unsigned char  reserved1;
    unsigned char  boot_signature;
    unsigned int   volume_id;
    unsigned char  volume_label[11];
    unsigned char  fs_type[8];        // "FAT32   "
} __attribute__((packed));

// =============================================
// Entrée de répertoire (32 octets)
// =============================================
struct FAT32_DirEntry {
    unsigned char  name[8];           // Nom (padded avec espaces)
    unsigned char  ext[3];            // Extension
    unsigned char  attributes;        // 0x10 = dossier, 0x20 = fichier
    unsigned char  reserved;
    unsigned char  create_time_tenth;
    unsigned short create_time;
    unsigned short create_date;
    unsigned short access_date;
    unsigned short cluster_high;      // Bits hauts du cluster
    unsigned short modify_time;
    unsigned short modify_date;
    unsigned short cluster_low;       // Bits bas du cluster
    unsigned int   file_size;
} __attribute__((packed));

// Attributs
static const unsigned char FAT_ATTR_READONLY  = 0x01;
static const unsigned char FAT_ATTR_HIDDEN    = 0x02;
static const unsigned char FAT_ATTR_SYSTEM    = 0x04;
static const unsigned char FAT_ATTR_VOLUME_ID = 0x08;
static const unsigned char FAT_ATTR_DIRECTORY = 0x10;
static const unsigned char FAT_ATTR_ARCHIVE   = 0x20;
static const unsigned char FAT_ATTR_LFN       = 0x0F; // Long filename

// =============================================
// Fichier ouvert
// =============================================
struct FAT32_File {
    bool     valid;
    unsigned int   cluster;      // Cluster de départ
    unsigned int   file_size;    // Taille en octets
    unsigned int   position;     // Position de lecture courante
    char     name[13];           // "FILENAME.EXT\0"
};

// =============================================
// Driver FAT32
// =============================================
class FAT32 {
    DiskDrive* disk;
    FAT32_BPB  bpb;
    bool       mounted = false;

    // Infos calculées depuis le BPB
    unsigned int fat_start;      // LBA du début de la FAT
    unsigned int data_start;     // LBA du début des données
    unsigned int root_cluster;   // Cluster du répertoire racine

    // Buffer secteur (512 octets)
    static unsigned char sector_buf[512];

    // Lire un secteur
    bool read_sector(unsigned int lba) {
        return disk->read(lba, 1, sector_buf);
    }

    // Calculer le LBA d'un cluster
    unsigned int cluster_to_lba(unsigned int cluster) {
        return data_start + (cluster - 2) * bpb.sectors_per_cluster;
    }

    // Lire l'entrée FAT pour un cluster (next cluster)
    unsigned int fat_next(unsigned int cluster) {
        unsigned int fat_offset  = cluster * 4;
        unsigned int fat_sector  = fat_start + (fat_offset / bpb.bytes_per_sector);
        unsigned int fat_index   = fat_offset % bpb.bytes_per_sector;

        if (!read_sector(fat_sector)) return 0x0FFFFFFF;

        unsigned int next = *(unsigned int*)(sector_buf + fat_index);
        return next & 0x0FFFFFFF;  // Masque les 4 bits hauts
    }

    // Comparer un nom FAT 8.3 avec un nom normal
    bool name_match(const FAT32_DirEntry* entry, const char* name) {
        char fat_name[13];
        int  j = 0;

        // Copier les 8 chars du nom
        for (int i = 0; i < 8 && entry->name[i] != ' '; i++)
            fat_name[j++] = entry->name[i];

        // Ajouter le point si extension non vide
        if (entry->ext[0] != ' ') {
            fat_name[j++] = '.';
            for (int i = 0; i < 3 && entry->ext[i] != ' '; i++)
                fat_name[j++] = entry->ext[i];
        }
        fat_name[j] = '\0';

        // Comparaison insensible à la casse
        for (int i = 0; fat_name[i] || name[i]; i++) {
            char a = fat_name[i];
            char b = name[i];
            if (a >= 'a' && a <= 'z') a -= 32;
            if (b >= 'a' && b <= 'z') b -= 32;
            if (a != b) return false;
        }
        return true;
    }
    // Ajoute cette méthode privée — cherche un nom dans un cluster de répertoire
bool find_in_dir(unsigned int cluster, const char* name, FAT32_DirEntry* out) {
    while (cluster < 0x0FFFFFF8) {
        unsigned int lba = cluster_to_lba(cluster);

        for (int s = 0; s < bpb.sectors_per_cluster; s++) {
            if (!read_sector(lba + s)) return false;

            FAT32_DirEntry* entries = (FAT32_DirEntry*)sector_buf;
            int count = bpb.bytes_per_sector / sizeof(FAT32_DirEntry);

            for (int i = 0; i < count; i++) {
                FAT32_DirEntry* e = &entries[i];
                if (e->name[0] == 0x00) return false;  // fin du répertoire
                if (e->name[0] == 0xE5) continue;       // supprimé
                if (e->attributes == FAT_ATTR_LFN) continue;

                if (name_match(e, name)) {
                    *out = *e;
                    return true;
                }
            }
        }
        cluster = fat_next(cluster);
    }
    return false;
}
unsigned int alloc_cluster() {
    // Lire la FAT secteur par secteur
    unsigned int fat_sectors = bpb.fat_size_32;

    for (unsigned int s = 0; s < fat_sectors; s++) {
        if (!read_sector(fat_start + s)) return 0;

        unsigned int* entries = (unsigned int*)sector_buf;
        int count = bpb.bytes_per_sector / 4;

        for (int i = 0; i < count; i++) {
            unsigned int val = entries[i] & 0x0FFFFFFF;
            if (val == 0x00000000) {
                // Cluster libre trouvé !
                unsigned int cluster = s * (bpb.bytes_per_sector / 4) + i;
                if (cluster < 2) continue;  // clusters 0 et 1 réservés
                return cluster;
            }
        }
    }
    return 0;  // disque plein
}

// Écrire une entrée dans la FAT
bool write_fat(unsigned int cluster, unsigned int value) {
    unsigned int fat_offset  = cluster * 4;
    unsigned int fat_sector  = fat_start + (fat_offset / bpb.bytes_per_sector);
    unsigned int fat_index   = fat_offset % bpb.bytes_per_sector;

    if (!read_sector(fat_sector)) return false;

    // Modifier l'entrée
    unsigned int* entry = (unsigned int*)(sector_buf + fat_index);
    *entry = (*entry & 0xF0000000) | (value & 0x0FFFFFFF);

    // Réécrire le secteur
    if (!disk->write(fat_sector, 1, sector_buf)) return false;

    // Mettre à jour la FAT2 si présente
    if (bpb.fat_count > 1) {
        unsigned int fat2_sector = fat_sector + bpb.fat_size_32;
        disk->write(fat2_sector, 1, sector_buf);
    }
    return true;
}

// Zéroer un cluster entier
bool zero_cluster(unsigned int cluster) {
    unsigned char zero_buf[512];
    for (int i = 0; i < 512; i++) zero_buf[i] = 0;

    unsigned int lba = cluster_to_lba(cluster);
    for (int s = 0; s < bpb.sectors_per_cluster; s++)
        if (!disk->write(lba + s, 1, zero_buf)) return false;
    return true;
}

// Écrire une entrée de répertoire dans un cluster
bool write_dir_entry(unsigned int cluster, int index, const FAT32_DirEntry* entry) {
    int entries_per_sector = bpb.bytes_per_sector / sizeof(FAT32_DirEntry);
    int sector_offset = index / entries_per_sector;
    int entry_offset  = index % entries_per_sector;

    unsigned int lba = cluster_to_lba(cluster) + sector_offset;
    if (!read_sector(lba)) return false;

    FAT32_DirEntry* entries = (FAT32_DirEntry*)sector_buf;
    entries[entry_offset] = *entry;

    return disk->write(lba, 1, sector_buf);
}

// Trouver un slot libre dans un répertoire
// Retourne l'index de l'entrée libre, ou -1 si plein
int find_free_slot(unsigned int cluster) {
    int index = 0;
    while (cluster < 0x0FFFFFF8) {
        unsigned int lba = cluster_to_lba(cluster);
        for (int s = 0; s < bpb.sectors_per_cluster; s++) {
            if (!read_sector(lba + s)) return -1;
            FAT32_DirEntry* entries = (FAT32_DirEntry*)sector_buf;
            int count = bpb.bytes_per_sector / sizeof(FAT32_DirEntry);
            for (int i = 0; i < count; i++) {
                if (entries[i].name[0] == 0x00 ||
                    entries[i].name[0] == 0xE5)
                    return index + i;
            }
            index += count;
        }
        cluster = fat_next(cluster);
    }
    return -1;
}

// Construire une entrée FAT32 depuis un nom "DIRNAME"
void make_dir_entry(FAT32_DirEntry* e, const char* name,
                    unsigned int cluster, bool is_dir, unsigned int size = 0) {
    // Remplir de spaces
    for (int i = 0; i < 8; i++) e->name[i] = ' ';
    for (int i = 0; i < 3; i++) e->ext[i]  = ' ';

    // Copier le nom en majuscules (max 8 chars)
    int i = 0;
    for (; name[i] && name[i] != '.' && i < 8; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        e->name[i] = c;
    }

    // Extension si présente
    if (name[i] == '.') {
        i++;
        for (int j = 0; name[i] && j < 3; i++, j++) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            e->ext[j] = c;
        }
    }

    e->attributes    = is_dir ? FAT_ATTR_DIRECTORY : FAT_ATTR_ARCHIVE;
    e->reserved      = 0;
    e->create_time_tenth = 0;
    e->create_time   = 0;
    e->create_date   = 0x5821;  // date fixe (à améliorer avec RTC plus tard)
    e->access_date   = 0x5821;
    e->modify_time   = 0;
    e->modify_date   = 0x5821;
    e->cluster_high  = (cluster >> 16) & 0xFFFF;
    e->cluster_low   = cluster & 0xFFFF;
    e->file_size     = size;
}

// Trouver le cluster du répertoire parent d'un chemin
unsigned int get_parent_cluster(const char* path, char* name_out) {
    // Ignorer préfixe volume
    if (path[1] == ':') path += 2;
    if (path[0] == '/' || path[0] == '\\') path++;

    unsigned int current = root_cluster;
    const char* remaining = path;

    while (true) {
        char component[13];
        const char* next;
        next_component(remaining, component, &next);

        if (*next == '\0') {
            // Dernier composant = le nom à créer
            for (int i = 0; component[i]; i++)
                name_out[i] = component[i];
            name_out[strlen(component)] = '\0';
            return current;
        }

        // Descendre dans le sous-répertoire
        FAT32_DirEntry entry;
        if (!find_in_dir(current, component, &entry)) return 0;
        if (!(entry.attributes & FAT_ATTR_DIRECTORY))  return 0;

        current = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
        remaining = next;
    }
}
// Normalise un séparateur \ en /
static char normalize(char c) {
    return c == '\\' ? '/' : c;
}

// Extrait le prochain composant d'un chemin
// "SYSTEM/HELLO.ELF" → composant="SYSTEM", reste="HELLO.ELF"
// "HELLO.ELF"        → composant="HELLO.ELF", reste=""
static void next_component(const char* path, char* component, const char** rest) {
    int i = 0;
    while (path[i] && normalize(path[i]) != '/') {
        component[i] = path[i];
        i++;
    }
    component[i] = '\0';

    if (normalize(path[i]) == '/')
        *rest = path + i + 1;
    else
        *rest = path + i;
}
public:
    // Créer un répertoire
    bool mkdir(const char* path) {
    if (!mounted) return false;

    char name[13];
    unsigned int parent_cluster = get_parent_cluster(path, name);
    if (!parent_cluster) return false;

    // 1. Allouer un cluster pour le nouveau répertoire
    unsigned int new_cluster = alloc_cluster();
    if (!new_cluster) return false;

    // 2. Marquer le cluster comme fin de chaîne dans la FAT
    if (!write_fat(new_cluster, 0x0FFFFFFF)) return false;

    // 3. Zéroer le cluster
    if (!zero_cluster(new_cluster)) return false;

    // 4. Créer les entrées "." et ".."
    FAT32_DirEntry dot, dotdot;

    make_dir_entry(&dot, ".", new_cluster, true);
    dot.name[0] = '.';
    for (int i = 1; i < 8; i++) dot.name[i] = ' ';

    make_dir_entry(&dotdot, "..", parent_cluster, true);
    dotdot.name[0] = '.';
    dotdot.name[1] = '.';
    for (int i = 2; i < 8; i++) dotdot.name[i] = ' ';

    if (!write_dir_entry(new_cluster, 0, &dot))    return false;
    if (!write_dir_entry(new_cluster, 1, &dotdot)) return false;

    // 5. Créer l'entrée dans le répertoire parent
    int slot = find_free_slot(parent_cluster);
    if (slot < 0) return false;

    FAT32_DirEntry new_entry;
    make_dir_entry(&new_entry, name, new_cluster, true);

    if (!write_dir_entry(parent_cluster, slot, &new_entry)) return false;

    return true;
}

// ============================================================
// RENAME — renommer un fichier ou dossier
// ============================================================
bool rename(const char* old_path, const char* new_name) {
    if (!mounted) return false;

    char old_name[13];
    unsigned int parent_cluster = get_parent_cluster(old_path, old_name);
    if (!parent_cluster) return false;

    // Trouver l'entrée existante
    unsigned int cluster = parent_cluster;
    while (cluster < 0x0FFFFFF8) {
        unsigned int lba = cluster_to_lba(cluster);
        for (int s = 0; s < bpb.sectors_per_cluster; s++) {
            if (!read_sector(lba + s)) return false;
            FAT32_DirEntry* entries = (FAT32_DirEntry*)sector_buf;
            int count = bpb.bytes_per_sector / sizeof(FAT32_DirEntry);
            for (int i = 0; i < count; i++) {
                if (entries[i].name[0] == 0x00) return false;
                if (entries[i].name[0] == 0xE5) continue;
                if (entries[i].attributes == FAT_ATTR_LFN) continue;
                if (name_match(&entries[i], old_name)) {
                    // Réécrire le nom
                    make_dir_entry(&entries[i], new_name,
                        ((unsigned int)entries[i].cluster_high << 16) | entries[i].cluster_low,
                        (entries[i].attributes & FAT_ATTR_DIRECTORY) != 0,
                        entries[i].file_size);
                    return disk->write(lba + s, 1, sector_buf);
                }
            }
        }
        cluster = fat_next(cluster);
    }
    return false;
}

// ============================================================
// DELETE — supprimer un fichier
// ============================================================
bool remove(const char* path) {
    if (!mounted) return false;

    char name[13];
    unsigned int parent_cluster = get_parent_cluster(path, name);
    if (!parent_cluster) return false;

    unsigned int cluster = parent_cluster;
    while (cluster < 0x0FFFFFF8) {
        unsigned int lba = cluster_to_lba(cluster);
        for (int s = 0; s < bpb.sectors_per_cluster; s++) {
            if (!read_sector(lba + s)) return false;
            FAT32_DirEntry* entries = (FAT32_DirEntry*)sector_buf;
            int count = bpb.bytes_per_sector / sizeof(FAT32_DirEntry);
            for (int i = 0; i < count; i++) {
                if (entries[i].name[0] == 0x00) return false;
                if (entries[i].name[0] == 0xE5) continue;
                if (entries[i].attributes == FAT_ATTR_LFN) continue;
                if (name_match(&entries[i], name)) {
                    // Libérer les clusters du fichier dans la FAT
                    unsigned int file_cluster =
                        ((unsigned int)entries[i].cluster_high << 16) |
                        entries[i].cluster_low;
                    while (file_cluster < 0x0FFFFFF8 && file_cluster >= 2) {
                        unsigned int next = fat_next(file_cluster);
                        write_fat(file_cluster, 0x00000000);  // libérer
                        file_cluster = next;
                    }
                    // Marquer l'entrée comme supprimée
                    entries[i].name[0] = 0xE5;
                    return disk->write(lba + s, 1, sector_buf);
                }
            }
        }
        cluster = fat_next(cluster);
    }
    return false;
}
    int get_attributes(const char* path) 
    {
    if (!mounted) return -1;
    if (!path || !path[0]) return FAT_ATTR_DIRECTORY; // racine
    return -1; // à implémenter : trouver le fichier et retourner ses attributs    
    }
    bool mount(ATADriver* d) {
        disk = (DiskDrive*)d;

        // Lire le boot sector
        if (!read_sector(0)) return false;

        // Copier le BPB
        memcpy(&bpb, sector_buf, sizeof(FAT32_BPB));

        // Vérifier signature FAT32
        if (bpb.bytes_per_sector != 512) return false;
        if (bpb.fat_size_32 == 0)        return false;

        // Calculer les positions
        fat_start    = bpb.reserved_sectors;
        data_start   = fat_start + bpb.fat_count * bpb.fat_size_32;
        root_cluster = bpb.root_cluster;

        mounted = true;
        return true;
    }

    // Chercher un fichier dans le répertoire racine
    FAT32_File open(const char* path) {
    FAT32_File f;
    f.valid = false;
    if (!mounted) return f;

    // Ignorer le préfixe de volume "X:/" ou "X:\"
    if (path[0] && path[1] == ':')
        path += 2;

    // Ignorer le slash initial
    if (path[0] == '/' || path[0] == '\\')
        path++;

    // Traverser le chemin composant par composant
    unsigned int current_cluster = root_cluster;
    const char* remaining = path;

    

    while (true) {
        char component[13];
        const char* next;
        next_component(remaining, component, &next);

        if (component[0] == '\0') {
            f.valid = false;
            return f;
        }

        FAT32_DirEntry entry;
        if (!find_in_dir(current_cluster, component, &entry)) {
            f.valid = false;  // composant non trouvé
            return f;
        }

        if (*next == '\0') {
            // Dernier composant — c'est le fichier qu'on cherche
            if (entry.attributes & FAT_ATTR_DIRECTORY) {
                f.valid = false;  // c'est un dossier, pas un fichier
                return f;
            }
            f.valid     = true;
            f.cluster   = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
            f.file_size = entry.file_size;
            f.position  = 0;

            // Copier le nom
            int j = 0;
            for (int k = 0; k < 8 && entry.name[k] != ' '; k++)
                f.name[j++] = entry.name[k];
            if (entry.ext[0] != ' ') {
                f.name[j++] = '.';
                for (int k = 0; k < 3 && entry.ext[k] != ' '; k++)
                    f.name[j++] = entry.ext[k];
            }
            f.name[j] = '\0';
            return f;

        } else {
            // Composant intermédiaire — doit être un dossier
            if (!(entry.attributes & FAT_ATTR_DIRECTORY)) {
                f.valid = false;  // pas un dossier
                return f;
            }
            // Descendre dans le sous-répertoire
            current_cluster = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
            remaining = next;
        }
    }
}

    // Lire le contenu d'un fichier en entier
    bool read_file(FAT32_File* f, void* buffer, unsigned int max_size) {
        if (!f->valid || !mounted) return false;

        unsigned char* buf  = (unsigned char*)buffer;
        unsigned int   read = 0;
        unsigned int   cluster = f->cluster;

        while (cluster < 0x0FFFFFF8 && read < f->file_size && read < max_size) {
            unsigned int lba = cluster_to_lba(cluster);

            for (int s = 0; s < bpb.sectors_per_cluster && read < f->file_size; s++) {
                if (!read_sector(lba + s)) return false;

                unsigned int to_copy = 512;
                if (read + to_copy > f->file_size) to_copy = f->file_size - read;
                if (read + to_copy > max_size)     to_copy = max_size - read;

                memcpy(buf + read, sector_buf, to_copy);
                read += to_copy;
            }
            cluster = fat_next(cluster);
        }
        return true;
    }
    // Vérifie si un chemin est un répertoire
    bool is_directory(const char* path) {
    if (!mounted) return false;
    if (!path || !path[0]) return true;  // racine = toujours un dossier

    // Ignorer le préfixe volume "X:/"
    if (path[1] == ':') path += 2;
    if (path[0] == '/' || path[0] == '\\') path++;

    // Traverser le chemin composant par composant
    unsigned int current_cluster = root_cluster;
    const char* remaining = path;

    while (true) {
        char component[13];
        const char* next;
        next_component(remaining, component, &next);

        if (!component[0]) return false;

        FAT32_DirEntry entry;
        if (!find_in_dir(current_cluster, component, &entry))
            return false;  // composant introuvable

        if (*next == '\0') {
            // Dernier composant — vérifier que c'est un dossier
            return (entry.attributes & FAT_ATTR_DIRECTORY) != 0;
        }

        // Composant intermédiaire — descendre dedans
        if (!(entry.attributes & FAT_ATTR_DIRECTORY))
            return false;  // pas un dossier

        current_cluster = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
        remaining = next;
    }
}


    
    // Lister les fichiers du répertoire racine
    void list_files(Terminal* term, const char* path = "") {
    if (!mounted) { term->println("No filesystem mounted."); return; }

    // Trouver le cluster de départ
    unsigned int cluster = root_cluster;

    // Si un chemin est donné, naviguer jusqu'au dossier
    if (path && path[0]) {
        // Ignorer préfixe volume
        if (path[1] == ':') path += 2;
        if (*path == '/' || *path == '\\') path++;

        const char* remaining = path;
        while (*remaining) {
            char component[13];
            const char* next;
            next_component(remaining, component, &next);
            if (!component[0]) break;

            FAT32_DirEntry entry;
            if (!find_in_dir(cluster, component, &entry)) {
                term->println("Directory not found.");
                return;
            }
            if (!(entry.attributes & FAT_ATTR_DIRECTORY)) {
                term->println("Not a directory.");
                return;
            }
            cluster = ((unsigned int)entry.cluster_high << 16) | entry.cluster_low;
            remaining = next;
        }
    }

    // Lister le contenu du cluster trouvé
    // ... (même code qu'avant mais avec `cluster` au lieu de `root_cluster`)
    while (cluster < 0x0FFFFFF8) {
        unsigned int lba = cluster_to_lba(cluster);
        for (int s = 0; s < bpb.sectors_per_cluster; s++) {
            if (!read_sector(lba + s)) return;
            FAT32_DirEntry* entries = (FAT32_DirEntry*)sector_buf;
            int count = bpb.bytes_per_sector / sizeof(FAT32_DirEntry);
            for (int i = 0; i < count; i++) {
                FAT32_DirEntry* e = &entries[i];
                if (e->name[0] == 0x00) return;
                if (e->name[0] == 0xE5) continue;
                if (e->attributes == FAT_ATTR_LFN) continue;
                if (e->name[0] == '.') continue;  // ignorer . et ..

                char line[13];
                int j = 0;
                for (int k = 0; k < 8 && e->name[k] != ' '; k++)
                    line[j++] = e->name[k];
                if (e->ext[0] != ' ') {
                    line[j++] = '.';
                    for (int k = 0; k < 3 && e->ext[k] != ' '; k++)
                        line[j++] = e->ext[k];
                }
                line[j] = '\0';

                if (e->attributes & FAT_ATTR_DIRECTORY) {
                    term->set_color(0xB);  // cyan pour les dossiers
                    term->print("[DIR] ");
                } else {
                    term->set_color(0xF);  // blanc pour les fichiers
                    term->print("      ");
                }
                term->print(line);

                if (!(e->attributes & FAT_ATTR_DIRECTORY)) {
                    term->print("  ");
                    char sz[16];
                    ulltoa(e->file_size, sz);
                    term->print(sz);
                    term->print(" bytes");
                }
                term->set_color(0xF);
                term->putchar('\n');
            }
        }
        cluster = fat_next(cluster);
    }
}

    bool is_mounted() { return mounted; }
};

// Définition du buffer statique
unsigned char FAT32::sector_buf[512];