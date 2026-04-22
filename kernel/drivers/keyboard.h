#pragma once
#include "io.h"

// Table de conversion scancode → caractère ASCII (clavier QWERTY)
static const char SCANCODE_MAP[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,   0,   0,   0,   '-', 0,   0,   0,   '+', 0,   0,   0,
    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

class Keyboard {
public:
    char getchar() {
        while (true) {
            // Attendre données disponibles
            if (!(inb(0x64) & 0x01)) continue;

            // Vérifier la SOURCE avant de lire
            unsigned char status = inb(0x64);
            unsigned char data   = inb(0x60);  // lire dans tous les cas pour vider le buffer

            // Bit 5 = données souris → ignorer
            if (status & 0x20) continue;

            // Ignorer key-release
            if (data & 0x80) continue;

            char c = SCANCODE_MAP[data];
            if (c) return c;
        }
    }

    char poll() {
        if (!(inb(0x64) & 0x01)) return 0;

        unsigned char status = inb(0x64);
        unsigned char data   = inb(0x60);

        // Données souris → ignorer
        if (status & 0x20) return 0;
        if (data & 0x80)   return 0;

        return SCANCODE_MAP[data];
    }
};