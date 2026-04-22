#pragma once

// Structure d'une entrée IDT 64 bit
struct IDT_Entry {
    unsigned short offset_low;   // bits 0-15 de l'adresse
    unsigned short selector;     // segment code (0x08)
    unsigned char  ist;          // interrupt stack table (0)
    unsigned char  flags;        // type + DPL + present
    unsigned short offset_mid;   // bits 16-31
    unsigned int   offset_high;  // bits 32-63
    unsigned int   zero;
} __attribute__((packed));

struct IDT_Pointer {
    unsigned short limit;
    unsigned long long base;
} __attribute__((packed));

// Déclarations ASM
extern IDT_Entry idt_table[256];
extern IDT_Pointer idt_pointer;
extern "C" void idt_load();
extern "C" void isr_0x80();
extern "C" void isr_0();
extern "C" void isr_6();
extern "C" void isr_8();
extern "C" void isr_13();
extern "C" void isr_14();
extern "C" void isr_0x80();

class IDT {
public:
    void set_entry(int num, void* handler) {
        unsigned long long addr = (unsigned long long)handler;

        idt_table[num].offset_low  = addr & 0xFFFF;
        idt_table[num].selector    = 0x08;          // segment code kernel
        idt_table[num].ist         = 0;
        idt_table[num].flags       = 0x8E;          // present + interrupt gate
        idt_table[num].offset_mid  = (addr >> 16) & 0xFFFF;
        idt_table[num].offset_high = (addr >> 32) & 0xFFFFFFFF;
        idt_table[num].zero        = 0;
    }

    void init() {
        // Enregistrer le handler syscall sur l'interrupt 0x80
        set_entry(0x80, (void*)isr_0x80);
        // Initialiser les fautes génériques (page fault, general protection fault, etc.) sur des handlers par défaut qui font un kernel panic
        set_entry(0,    (void*)isr_0);     // Division par zéro
        set_entry(6,    (void*)isr_6);     // Instruction invalide
        set_entry(8,    (void*)isr_8);     // Double fault
        set_entry(13,   (void*)isr_13);    // GPF
        set_entry(14,   (void*)isr_14);    // Page fault
        set_entry(0x80, (void*)isr_0x80);  // Syscall
        // Charger l'IDT
        idt_load();
    }
};