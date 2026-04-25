#include "../../kernel/apicore/kapi.h"

// le système explose à chaque fois que je tente l'init du driver j'en ai marre.

struct SB16State {
    bool          present;
    unsigned short base;
};

// Initialisées à nullptr/0 explicitement
static KernelAPI* g_api = nullptr;
static SB16State  sb16  = { false, 0 };

static void dsp_write(unsigned char val) {
    for (int i = 0; i < 100000; i++)
        if (!(g_api->inb(sb16.base + 0xC) & 0x80)) break;
    g_api->outb(sb16.base + 0xC, val);
}

static unsigned char dsp_read() {
    for (int i = 0; i < 100000; i++)
        if (g_api->inb(sb16.base + 0xE) & 0x80) break;
    return g_api->inb(sb16.base + 0xA);
}

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
    sb16.base    = 0x220;
    sb16.present = false;

    g_api->print("SB16: initializing... ");

    // Reset DSP
    g_api->outb(sb16.base + 0x6, 1);
    for (volatile int i = 0; i < 1000; i++);
    g_api->outb(sb16.base + 0x6, 0);

    unsigned char resp = dsp_read();
    if (resp != 0xAA) {
        g_api->println("not found.");
        return 1;
    }

    dsp_write(0xE1);
    unsigned char major = dsp_read();
    dsp_read();

    if (major < 4) {
        g_api->println("not SB16.");
        return 2;
    }

    sb16.present = true;

    // Volume master
    g_api->outb(sb16.base + 0x4, 0x22);
    g_api->outb(sb16.base + 0x5, 0xCC);

    g_api->println("OK");
    g_api->register_device("sb16", &sb16, DEV_AUDIO);

    return 0;
}