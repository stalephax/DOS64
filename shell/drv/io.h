#pragma once
// Fonctions d'accès aux ports I/O
#include "../std.h"
// Lire un octet depuis un port I/O
static inline unsigned char inb(unsigned short port) {
    unsigned char val;
    asm volatile ("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

// Écrire un octet sur un port I/O
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Lire un mot (2 octets) depuis un port I/O
static inline unsigned short inw(unsigned short port) {
    unsigned short val;
    asm volatile ("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
// Écrire un mot (2 octets) sur un port I/O
static inline void outw(unsigned short port, unsigned short val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
