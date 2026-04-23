#pragma once
#include "memory.h"
#include "standart.h"
#include "syscall.h"
// =============================================
// Structures ELF64
// =============================================
struct ELF64_Header {
    unsigned char  magic[4];     // 0x7F 'E' 'L' 'F'
    unsigned char  bits;         // 2 = 64 bit
    unsigned char  endian;       // 1 = little endian
    unsigned char  elf_version;
    unsigned char  os_abi;
    unsigned char  padding[8];
    unsigned short type;         // 2 = executable
    unsigned short machine;      // 0x3E = x86_64
    unsigned int   version;
    unsigned long long entry;    // point d'entrée
    unsigned long long phoff;    // offset de la program header table
    unsigned long long shoff;    // offset de la section header table
    unsigned int   flags;        // 0 pour x86_64, serviront a plus pour ajouter des FLAGS de compatibilité avec Linux, Windows moderne etc.
    unsigned short ehsize;
    unsigned short phentsize;    // taille d'une program header entry
    unsigned short phnum;        // nombre de program headers
    unsigned short shentsize;
    unsigned short shnum;
    unsigned short shstrndx;
} __attribute__((packed));

struct ELF64_ProgramHeader {
    unsigned int   type;         // 1 = PT_LOAD (à charger en mémoire)
    unsigned int   flags;        // R=4, W=2, X=1
    unsigned long long offset;   // offset dans le fichier
    unsigned long long vaddr;    // adresse virtuelle cible
    unsigned long long paddr;    // adresse physique (ignorée)
    unsigned long long filesz;   // taille dans le fichier
    unsigned long long memsz;    // taille en mémoire (peut être > filesz)
    unsigned long long align;    // alignement
} __attribute__((packed));

// Types ELF
static const unsigned int PT_LOAD = 1;

class ELF64Loader {
    HeapAllocator* heap;

public:
    ELF64Loader(HeapAllocator* h) : heap(h) {}

    // Vérifier si un buffer contient un ELF64 valide
    bool is_valid(const unsigned char* data) {
        if (data[0] != 0x7F || data[1] != 'E' ||
            data[2] != 'L'  || data[3] != 'F') return false;
        if (data[4] != 2) return false;  // pas 64 bit
        return true;
    }

    // Charger et exécuter un ELF64
    // data = buffer contenant le fichier ELF complet
    // size = taille du buffer
    // retourne le code de sortie
    int load_and_run(unsigned char* data, unsigned int size) {
        if (!is_valid(data)) return -1;

        ELF64_Header* hdr = (ELF64_Header*)data;

        // Vérifier que c'est un exécutable x86_64
        if (hdr->type    != 2)      return -2;
        if (hdr->machine != 0x3E)   return -3;

        // Charger chaque segment PT_LOAD
        for (int i = 0; i < hdr->phnum; i++) {
            ELF64_ProgramHeader* ph = (ELF64_ProgramHeader*)(
                data + hdr->phoff + i * hdr->phentsize
            );

            if (ph->type != PT_LOAD) continue;

            // Copier le segment à son adresse virtuelle
            unsigned char* dest = (unsigned char*)ph->vaddr;
            unsigned char* src  = data + ph->offset;

            memcpy(dest, src, ph->filesz);

            // Zéroer la partie BSS (memsz > filesz)
            if (ph->memsz > ph->filesz)
                memset(dest + ph->filesz, 0, ph->memsz - ph->filesz);
        }

        // Appeler le point d'entrée
        typedef int (*EntryPoint)();
        EntryPoint entry = (EntryPoint)hdr->entry;

        current_program.running   = true;
        current_program.exit_code = 0;

        int ret = entry();  // ← le programme s'exécute ici

        return ret;
    }
};