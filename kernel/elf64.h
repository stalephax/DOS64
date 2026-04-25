#pragma once
#include "memory.h"
#include "standart.h"
#include "syscall.h"
#include "apicore/kapi.h"
// =============================================
// Exécutif ELF64
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

static void restore_text_mode() {
    // Réécrire les registres VGA pour le mode texte 80x25
    // Port 0x3C2 — Misc Output Register
    outb(0x3C2, 0x67);

    // Sequencer
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x00);
    outb(0x3C4, 0x02); outb(0x3C5, 0x03);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x02);

    // Déverrouiller CRTC
    outb(0x3D4, 0x11); outb(0x3D5, inb(0x3D5) & ~0x80);

    // CRTC registres mode texte 80x25
    static const unsigned char crtc[] = {
        0x5F,0x4F,0x50,0x82,0x55,0x81,0xBF,0x1F,
        0x00,0x4F,0x0D,0x0E,0x00,0x00,0x00,0x00,
        0x9C,0x8E,0x8F,0x28,0x1F,0x96,0xB9,0xA3,0xFF
    };
    for (int i = 0; i < 25; i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc[i]);
    }

    // Graphics controller — mode texte
    static const unsigned char gc[] = {
        0x00,0x00,0x00,0x00,0x00,0x10,0x0E,0x00,0xFF
    };
    for (int i = 0; i < 9; i++) {
        outb(0x3CE, i);
        outb(0x3CF, gc[i]);
    }

    // Attribute controller — mode texte
    inb(0x3DA);
    static const unsigned char ac[] = {
        0x00,0x01,0x02,0x03,0x04,0x05,0x14,0x07,
        0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
        0x0C,0x00,0x0F,0x08,0x00
    };
    for (int i = 0; i < 21; i++) {
        outb(0x3C0, i);
        outb(0x3C0, ac[i]);
    }
    outb(0x3C0, 0x20);

    // Réinitialiser le terminal
    if (term) term->clear();
}

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
        restore_text_mode(); // Revenir en mode texte après l'exécution

        return ret;
    }

    // pilotes .SYS
    int load_and_call_driver(unsigned char* data, unsigned int size, void* kapi) {
        
        KernelAPI* api = (KernelAPI*)kapi;
        char dbg[32];
        ulltoa((unsigned long long)api->print, dbg);
        term->print("kapi->print = 0x"); term->println(dbg);
        ulltoa((unsigned long long)api->outb, dbg);
        term->print("kapi->outb  = 0x"); term->println(dbg);
        ulltoa((unsigned long long)api->inb, dbg);
        term->print("kapi->inb   = 0x"); term->println(dbg);

    if (!is_valid(data)) return -1;
    
    ELF64_Header* hdr = (ELF64_Header*)data;
    if (hdr->type != 2 || hdr->machine != 0x3E) return -2;
    // zéro sur le BSS
    for (int i = 0; i < hdr->phnum; i++) {
    ELF64_ProgramHeader* ph = (ELF64_ProgramHeader*)(
        data + hdr->phoff + i * hdr->phentsize);
    if (ph->type != PT_LOAD) continue;

    memcpy((void*)ph->vaddr, data + ph->offset, ph->filesz);

    // Zéroer la BSS — OBLIGATOIRE pour les globales
    if (ph->memsz > ph->filesz)
        memset((void*)(ph->vaddr + ph->filesz), 0,
               ph->memsz - ph->filesz);
    }
    // Charger les segments
    for (int i = 0; i < hdr->phnum; i++) {
        ELF64_ProgramHeader* ph = (ELF64_ProgramHeader*)(
            data + hdr->phoff + i * hdr->phentsize);
        if (ph->type != PT_LOAD) continue;
        memcpy((void*)ph->vaddr, data + ph->offset, ph->filesz);
        if (ph->memsz > ph->filesz)
            memset((void*)(ph->vaddr + ph->filesz), 0, ph->memsz - ph->filesz);
    }

    // Appeler driver_entry(KernelAPI*)
    typedef int (*DriverEntry)(void*);
    DriverEntry entry = (DriverEntry)hdr->entry;
    return entry(kapi);
}
};