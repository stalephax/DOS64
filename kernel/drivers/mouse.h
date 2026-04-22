#pragma once
#include "drivers/io.h"

class PS2Mouse {
    int x, y;
    bool left, right, middle;
    bool present = false;

    // Dans mouse.h, adapte les limites pour le mode texte :
    static const int SCREEN_W = 80;   // colonnes
    static const int SCREEN_H = 25;   // lignes

    void wait_write() {
        for (int i = 0; i < 100000; i++)
            if (!(inb(0x64) & 0x02)) return;
    }

    void wait_read() {
        for (int i = 0; i < 100000; i++)
            if (inb(0x64) & 0x01) return;
    }

    void write(unsigned char val) {
        wait_write();
        outb(0x64, 0xD4);  // Dire au contrôleur PS/2 qu'on écrit à la souris
        wait_write();
        outb(0x60, val);
    }

    unsigned char read() {
        wait_read();
        return inb(0x60);
    }

public:
    bool init() {
        x = 160; y = 100;  // Centre de l'écran

        // Activer le port auxiliaire (souris)
        wait_write();
        outb(0x64, 0xA8);

        // Activer les interruptions souris
        wait_write();
        outb(0x64, 0x20);
        wait_read();
        unsigned char status = inb(0x60) | 0x02;
        wait_write();
        outb(0x64, 0x60);
        wait_write();
        outb(0x60, status);

        // Utiliser les paramètres par défaut
        write(0xF6);
        read();  // ACK

        // Activer le streaming
        write(0xF4);
        read();  // ACK

        present = true;
        return true;
    }

    // Lire un paquet souris (3 octets) — non bloquant
    bool poll(int& dx, int& dy, bool& btn_left, bool& btn_right) {
    if (!(inb(0x64) & 0x01)) return false;

    // Vérifier que ce sont bien des données souris (bit 5 = 1)
    unsigned char status = inb(0x64);
    if (!(status & 0x20)) return false;  // données clavier → pas touche

    unsigned char b1 = inb(0x60);
    if (!(b1 & 0x08)) return false;

    // Attendre et lire les 2 octets suivants
    // mais vérifier que ce sont aussi des données souris
    for (int t = 0; t < 100000; t++) if (inb(0x64) & 0x01) break;
    unsigned char b2 = inb(0x60);

    for (int t = 0; t < 100000; t++) if (inb(0x64) & 0x01) break;
    unsigned char b3 = inb(0x60);

    btn_left  = b1 & 0x01;
    btn_right = b1 & 0x02;

    dx = (int)b2 - ((b1 & 0x10) ? 256 : 0);
    dy = (int)b3 - ((b1 & 0x20) ? 256 : 0);
    dy = -dy;

    x += dx; y += dy;
    if (x < 0) x = 0;  if (x >= SCREEN_W) x = SCREEN_W - 1;
    if (y < 0) y = 0;  if (y >= SCREEN_H) y = SCREEN_H - 1;

    return true;
}

    int get_x()     { return x; }
    int get_y()     { return y; }
    bool is_present() { return present; }
};