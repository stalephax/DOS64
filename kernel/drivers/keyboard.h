#pragma once
#include "io.h"

// ============================================================
// Tables de scancodes — 4 layouts
// ============================================================

// QWERTY minuscules
static const char QWERTY_LOWER[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',
    '\b','\t','q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',
    '\n', 0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';','\'', '`',
    0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,   '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,   0,   0,   0,  '-',  0,   0,   0,  '+',  0,   0,   0,
    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// QWERTY majuscules (Shift)
static const char QWERTY_UPPER[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b','\t','Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,   '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,   0,   0,   0,  '-',  0,   0,   0,  '+',  0,   0,   0,
    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// AZERTY minuscules
static const char AZERTY_LOWER[128] = {
    0,   27,  '&', 'e', '"', '\'','(', '-', 'e', '_', 'c', 'a', ')', '=',
    '\b','\t','a', 'z', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '^', '$',
    '\n', 0,  'q', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 'u', '`',
    0,   '*', 'w', 'x', 'c', 'v', 'b', 'n', ',', ';', ':', '!', 0,   '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,   0,   0,   0,  '-',  0,   0,   0,  '+',  0,   0,   0,
    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// AZERTY majuscules (Shift)
static const char AZERTY_UPPER[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', ']', '+',
    '\b','\t','A', 'Z', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '^', '$',
    '\n', 0,  'Q', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 'M', '%', '~',
    0,   'u', 'W', 'X', 'C', 'V', 'B', 'N', '?', '.', '/', '!', 0,   '*',
    0,   ' ',  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,   0,   0,   0,  '-',  0,   0,   0,  '+',  0,   0,   0,
    0,   0,    0,   0,   0,   0,   0,   0,   0,   0,   0,   0
};

// ============================================================
// Scancodes spéciaux
// ============================================================
static const unsigned char SC_LSHIFT    = 0x2A;
static const unsigned char SC_RSHIFT    = 0x36;
static const unsigned char SC_LSHIFT_R  = 0xAA;  // release
static const unsigned char SC_RSHIFT_R  = 0xB6;  // release
static const unsigned char SC_CAPSLOCK  = 0x3A;
static const unsigned char SC_CTRL      = 0x1D;
static const unsigned char SC_ALT       = 0x38;

// ============================================================
// Layout sélectionnable
// ============================================================
enum KeyboardLayout {
    LAYOUT_QWERTY = 0,
    LAYOUT_AZERTY = 1
};

class Keyboard {
    bool shift    = false;
    bool capslock = false;
    bool ctrl     = false;
    bool alt      = false;
    KeyboardLayout layout = LAYOUT_QWERTY;

    // Sélectionner la bonne table selon l'état
    char translate(unsigned char scancode) {
        bool upper = shift ^ capslock;  // XOR : shift OU capslock mais pas les deux

        const char* table;
        if (layout == LAYOUT_AZERTY)
            table = upper ? AZERTY_UPPER : AZERTY_LOWER;
        else
            table = upper ? QWERTY_UPPER : QWERTY_LOWER;

        return table[scancode];
    }

    // Gérer les touches modificatrices
    // Retourne true si c'était une touche modificatrice
    bool handle_modifier(unsigned char scancode) {
        switch (scancode) {
            case SC_LSHIFT:
            case SC_RSHIFT:   shift = true;  return true;
            case SC_LSHIFT_R:
            case SC_RSHIFT_R: shift = false; return true;
            case SC_CAPSLOCK: capslock = !capslock; return true;
            case SC_CTRL:     ctrl = true;   return true;
            case SC_ALT:      alt  = true;   return true;
            // releases de ctrl/alt
            case 0x9D:        ctrl = false;  return true;
            case 0xB8:        alt  = false;  return true;
        }
        return false;
    }

public:
    void set_layout(KeyboardLayout l) { layout = l; }
    KeyboardLayout get_layout()       { return layout; }
    bool is_shift()    { return shift; }
    bool is_capslock() { return capslock; }
    bool is_ctrl()     { return ctrl; }

    char getchar() {
        while (true) {
            if (!(inb(0x64) & 0x01)) continue;

            unsigned char status   = inb(0x64);
            unsigned char scancode = inb(0x60);

            // Ignorer données souris
            if (status & 0x20) continue;

            // Gérer les modificateurs (shift, caps, ctrl...)
            if (handle_modifier(scancode)) continue;

            // Ignorer key-release (bit 7)
            if (scancode & 0x80) continue;

            char c = translate(scancode);
            if (c) return c;
        }
    }

    char poll() {
        if (!(inb(0x64) & 0x01)) return 0;

        unsigned char status   = inb(0x64);
        unsigned char scancode = inb(0x60);

        if (status & 0x20) return 0;

        if (handle_modifier(scancode)) return 0;
        if (scancode & 0x80) return 0;

        return translate(scancode);
    }
};