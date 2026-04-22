#pragma once
#include "drivers/io.h"
#include "drivers/beep.h"
// faute grave sur le CPU? pas de problème avec panic, on affiche un joli écran rouge avec les infos du CPU et on halt la machine
static unsigned short* const PANIC_VGA = (unsigned short*)0xB8000;

static void panic_print(const char* str, int line, unsigned char color = 0x4F) {
    for (int i = 0; i < 80; i++)
        PANIC_VGA[line * 80 + i] = (color << 8) | ' ';
    for (int i = 0; str[i] && i < 80; i++)
        PANIC_VGA[line * 80 + i] = (unsigned short)str[i] | (color << 8);
}

static void panic_printhex(unsigned long long val, int line, int col, unsigned char color = 0x4F) {
    const char* hex = "0123456789ABCDEF";
    char buf[19] = "0x0000000000000000";
    for (int i = 15; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    for (int i = 0; buf[i] && col + i < 80; i++)
        PANIC_VGA[line * 80 + col + i] = (unsigned short)buf[i] | (color << 8);
}

// Lire les registres CPU importants
static unsigned long long read_cr0() { unsigned long long v; asm volatile("mov %%cr0, %0" : "=r"(v)); return v; }
static unsigned long long read_cr2() { unsigned long long v; asm volatile("mov %%cr2, %0" : "=r"(v)); return v; }
static unsigned long long read_cr3() { unsigned long long v; asm volatile("mov %%cr3, %0" : "=r"(v)); return v; }
static unsigned long long read_rsp() { unsigned long long v; asm volatile("mov %%rsp, %0" : "=r"(v)); return v; }
static unsigned long long read_rip() { unsigned long long v; asm volatile("lea (%%rip), %0" : "=r"(v)); return v; }

__attribute__((noreturn))
static void kernel_panic(const char* reason, unsigned long long code = 0) {
    asm volatile(
        "mov $0x00110000, %%rsp\n"  // adresse fixe sous le kernel
        ::: "memory"
    );
    asm volatile("cli");
    // Fond rouge sur tout l'écran
    for (int i = 0; i < 80 * 25; i++)
        PANIC_VGA[i] = 0x4F20;  // rouge + espace

    // Titre
    panic_print("*** DOS64 KERNEL PANIC ***", 0);
    panic_print("your computer fcked up and has to stop what it was doing.", 1);
    panic_print(reason, 2);

    // Code d'erreur
    panic_print("Error code:", 3);
    panic_printhex(code, 3, 12);

    // Dump des registres
    panic_print("--- CPU REGISTERS ---", 5);

    panic_print("CR0:", 6);  panic_printhex(read_cr0(), 6, 5);
    panic_print("CR2:", 7);  panic_printhex(read_cr2(), 7, 5);  // page fault addr
    panic_print("CR3:", 8);  panic_printhex(read_cr3(), 8, 5);  // page tables
    panic_print("RSP:", 9);  panic_printhex(read_rsp(), 9, 5);  // stack pointer
    panic_print("RIP:", 10); panic_printhex(read_rip(), 10, 5); // instruction pointer

    panic_print("--- SYSTEM HALTED --- Please reboot with that power button", 12);

    // Halt
    asm volatile("cli; hlt");
    while(true) {}
}