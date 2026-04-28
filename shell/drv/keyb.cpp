// ============================================================
// Tables de scancodes — 4 layouts
// ============================================================
// sera compilable en un driver SYS, mais pour l'instant c'est plus simple de le garder dans le kernel
#include "../../kernel/apicore/kapi.h"
#include "io.h"
// Stroupe-Strupe
struct KeyboardState {
    bool          present;
    unsigned short base;
};
static KernelAPI* g_api = nullptr;
static KeyboardState  keyb  = { false, 0 };
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

extern "C" __attribute__((visibility("default"))) int driver_entry(KernelAPI* api) {
    // 1. Initialiser le pointeur API en premier
    g_api = api;

    // 2. Vérifier les pointeurs critiques
    if (!g_api)               return -10;
    if (!g_api->print)        return -11;
    if (!g_api->outb)         return -12;
    if (!g_api->inb)          return -13;
    if (!g_api->register_device) return -14;

    // 3. Initialiser l'état du device
    keyb.base    = 0x220;
    sb16.present = false;
    g_api->println(" Standart I/O Keyboard Driver | (C) Rational Systems ");
}


class Keyboard {
    bool shift    = false;
    bool capslock = false;
    bool ctrl     = false;
    bool alt      = false;
    KeyboardLayout layout = LAYOUT_QWERTY;



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

        return scancode;
    }
};