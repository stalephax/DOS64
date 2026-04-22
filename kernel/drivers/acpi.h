#pragma once
#include "drivers/io.h"

class ACPI {
    unsigned short pm1a_cnt = 0;  // Adresse du registre de contrôle ACPI
    unsigned short slp_typ  = 0;  // Valeur pour S5 (shutdown)
    bool available = false;

    // Chercher la RSDP dans la mémoire BIOS
    unsigned long long find_rsdp() {
        // Zone de recherche : 0xE0000 - 0xFFFFF
        for (unsigned long long addr = 0xE0000; addr < 0x100000; addr += 16) {
            if (*(unsigned long long*)addr == 0x2052545020445352ULL)  // "RSD PTR "
                return addr;
        }
        // Zone EBDA
        unsigned long long ebda = (unsigned long long)(*(unsigned short*)0x40E) << 4;
        for (unsigned long long addr = ebda; addr < ebda + 1024; addr += 16) {
            if (*(unsigned long long*)addr == 0x2052545020445352ULL)
                return addr;
        }
        return 0;
    }

public:
    bool init() {
        // Pour QEMU/VMWare, le port de shutdown ACPI est connu
        // QEMU : port 0x604, valeur 0x2000
        // VMWare : port 0x4004, valeur 0x3400
        // On essaie QEMU d'abord
        pm1a_cnt  = 0x604;
        slp_typ   = 0x2000;
        available = true;
        return true;
    }

    void shutdown() {
        if (!available) {
            // Fallback : triple fault volontaire
            asm volatile("cli; hlt");
            return;
        }
        // Écrire la commande de shutdown
        outw(pm1a_cnt, slp_typ | 0x2000);

        // Si on arrive ici, essayer VMWare
        outw(0x4004, 0x3400);

        // Dernier recours
        asm volatile("cli; hlt");
    }

    void reboot() {
        // Méthode universelle : pulse le port 0x64 (contrôleur PS/2)
        unsigned char good = 0x02;
        while (good & 0x02)
            good = inb(0x64);
        outb(0x64, 0xFE);  // Reset CPU
        asm volatile("cli; hlt");
    }
};