#pragma once
#include "io.h"

// ============================================================
// Structure d'un fichier .KTT (Keyboard Translation Table)
// ============================================================
struct KTT_Header {
    char         magic[4];      // "KTT1"
    char         name[32];      // "fr_FR", "en_US", etc.
    unsigned int version;
    unsigned char normal[128];  // scancode → ASCII normal
    unsigned char shifted[128]; // scancode → ASCII + Shift
    unsigned char altgr[128];   // scancode → ASCII + AltGr
} __attribute__((packed));

// Scancodes spéciaux
static const unsigned char SC_LSHIFT   = 0x2A;
static const unsigned char SC_RSHIFT   = 0x36;
static const unsigned char SC_LSHIFT_R = 0xAA;
static const unsigned char SC_RSHIFT_R = 0xB6;
static const unsigned char SC_CAPSLOCK = 0x3A;
static const unsigned char SC_CTRL     = 0x1D;
static const unsigned char SC_CTRL_R   = 0x9D;
static const unsigned char SC_ALT      = 0x38;
static const unsigned char SC_ALTGR    = 0xE0;
static const unsigned char SC_ALT_R    = 0xB8;

class Keyboard {
    bool shift    = false;
    bool capslock = false;
    bool ctrl     = false;
    bool alt      = false;
    bool altgr    = false;

    // Layout chargé en mémoire
    KTT_Header* layout    = nullptr;
    bool        has_layout = false;

    // Layout de fallback QWERTY hardcodé
    // (utilisé si aucun .KTT n'est chargé)
    static const char FALLBACK_NORMAL[128];
    static const char FALLBACK_SHIFTED[128];

    bool handle_modifier(unsigned char scancode) {
        switch (scancode) {
            case SC_LSHIFT:
            case SC_RSHIFT:   shift  = true;          return true;
            case SC_LSHIFT_R:
            case SC_RSHIFT_R: shift  = false;         return true;
            case SC_CAPSLOCK: capslock = !capslock;   return true;
            case SC_CTRL:     ctrl   = true;          return true;
            case SC_CTRL_R:   ctrl   = false;         return true;
            case SC_ALT:      alt    = true;          return true;
            case SC_ALT_R:    alt    = false;
                              altgr  = false;         return true;
            case 0xE0:        altgr  = true;          return true;
        }
        return false;
    }

    char translate(unsigned char scancode) {
        if (has_layout && layout) {
            // Utiliser le layout chargé
            if (altgr && layout->altgr[scancode])
                return (char)layout->altgr[scancode];
            bool upper = shift ^ capslock;
            if (upper)
                return (char)layout->shifted[scancode];
            return (char)layout->normal[scancode];
        }

        // Fallback QWERTY
        bool upper = shift ^ capslock;
        return upper ? FALLBACK_SHIFTED[scancode]
                     : FALLBACK_NORMAL[scancode];
    }

public:
    // Charger un layout depuis un buffer en mémoire
    bool load_layout(void* buf, unsigned int size) {
        if (size < sizeof(KTT_Header)) return false;

        KTT_Header* hdr = (KTT_Header*)buf;
        if (hdr->magic[0] != 'K' || hdr->magic[1] != 'T' ||
            hdr->magic[2] != 'T' || hdr->magic[3] != '1')
            return false;

        layout     = hdr;
        has_layout = true;
        return true;
    }

    void unload_layout() {
        layout     = nullptr;
        has_layout = false;
    }

    const char* get_layout_name() {
        if (has_layout && layout) return layout->name;
        return "QWERTY (builtin)";
    }

    char getchar() {
        while (true) {
            if (!(inb(0x64) & 0x01)) continue;
            unsigned char status   = inb(0x64);
            unsigned char scancode = inb(0x60);
            if (status & 0x20) continue;
            if (handle_modifier(scancode)) continue;
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

// Layout QWERTY hardcodé en fallback
const char Keyboard::FALLBACK_NORMAL[128] = {
    0,   27,  '1','2','3','4','5','6','7','8','9','0','-','=',
    '\b','\t','q','w','e','r','t','y','u','i','o','p','[',']',
    '\n', 0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/', 0,  '*',
    0,   ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,   0,  0,  0,  0, '-', 0,  0,  0, '+', 0,  0,  0,
    0,   0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

const char Keyboard::FALLBACK_SHIFTED[128] = {
    0,   27,  '!','@','#','$','%','^','&','*','(',')','_','+',
    '\b','\t','Q','W','E','R','T','Y','U','I','O','P','{','}',
    '\n', 0,  'A','S','D','F','G','H','J','K','L',':','"', '~',
    0,   '|','Z','X','C','V','B','N','M','<','>','?', 0,  '*',
    0,   ' ', 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,   0,   0,  0,  0,  0, '-', 0,  0,  0, '+', 0,  0,  0,
    0,   0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};