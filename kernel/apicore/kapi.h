#pragma once

// Table d'API kernel — passée aux drivers au chargement
struct KernelAPI {
    // Terminal
    void (*print)(const char* s);
    void (*println)(const char* s);
    void (*set_color)(unsigned char fg, unsigned char bg);

    // Mémoire
    void* (*malloc)(unsigned long long size);
    void  (*free)(void* ptr);

    // I/O ports
    unsigned char  (*inb)(unsigned short port);
    void           (*outb)(unsigned short port, unsigned char val);
    unsigned short (*inw)(unsigned short port);
    void           (*outw)(unsigned short port, unsigned short val);

    // Drivers — enregistrer un périphérique
    void (*register_device)(const char* name, void* device_ptr, int type);
};

// Types de périphériques
#define DEV_AUDIO   1
#define DEV_VIDEO   2
#define DEV_NETWORK 3
#define DEV_INPUT   4
#define DEV_STORAGE 5