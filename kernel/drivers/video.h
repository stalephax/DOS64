#pragma once
#include "io.h"

// VGA Mode 13h — 320x200 pixels, 256 couleurs
// Accessible directement en mémoire, pas besoin du BIOS en mode 64 bit
// On écrit directement dans les registres VGA

class VGAGraphics {
    static const int WIDTH  = 320;
    static const int HEIGHT = 200;
    unsigned char* const VMEM = (unsigned char*)0xA0000;

    // Écrire dans un registre VGA
    void write_reg(unsigned short port, unsigned char index, unsigned char val) {
        outb(port,     index);
        outb(port + 1, val);
    }

    // Séquence complète pour activer le mode 13h sans BIOS
    void set_mode13h() {
        // Déverrouiller les registres
        outb(0x3C2, 0x63);  // Misc output

        // Sequencer
        static const unsigned char seq[] = {
            0x03, 0x01, 0x0F, 0x00, 0x0E
        };
        for (int i = 0; i < 5; i++)
            write_reg(0x3C4, i, seq[i]);

        // CRTC — déverrouiller d'abord
        outb(0x3D4, 0x11);
        outb(0x3D5, inb(0x3D5) & ~0x80);

        static const unsigned char crtc[] = {
            0x5F,0x4F,0x50,0x82,0x54,0x80,0xBF,0x1F,
            0x00,0x41,0x00,0x00,0x00,0x00,0x00,0x00,
            0x9C,0x0E,0x8F,0x28,0x40,0x96,0xB9,0xA3,0xFF
        };
        for (int i = 0; i < 25; i++)
            write_reg(0x3D4, i, crtc[i]);

        // Graphics controller
        static const unsigned char gc[] = {
            0x00,0x00,0x00,0x00,0x00,0x40,0x05,0x0F,0xFF
        };
        for (int i = 0; i < 9; i++)
            write_reg(0x3CE, i, gc[i]);

        // Attribute controller
        inb(0x3DA);  // Reset flip-flop
        static const unsigned char ac[] = {
            0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
            0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
            0x41,0x00,0x0F,0x00,0x00
        };
        for (int i = 0; i < 21; i++) {
            outb(0x3C0, i);
            outb(0x3C0, ac[i]);
        }
        outb(0x3C0, 0x20);  // Enable video
    }

public:
    void init() {
        set_mode13h();
    }

    void set_pixel(int x, int y, unsigned char color) {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
        VMEM[y * WIDTH + x] = color;
    }

    unsigned char get_pixel(int x, int y) {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return 0;
        return VMEM[y * WIDTH + x];
    }

    void clear(unsigned char color = 0) {
        for (int i = 0; i < WIDTH * HEIGHT; i++)
            VMEM[i] = color;
    }

    void draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
        int dx = x2 - x1; if (dx < 0) dx = -dx;
        int dy = y2 - y1; if (dy < 0) dy = -dy;
        int sx = x1 < x2 ? 1 : -1;
        int sy = y1 < y2 ? 1 : -1;
        int err = dx - dy;

        while (true) {
            set_pixel(x1, y1, color);
            if (x1 == x2 && y1 == y2) break;
            int e2 = err * 2;
            if (e2 > -dy) { err -= dy; x1 += sx; }
            if (e2 <  dx) { err += dx; y1 += sy; }
        }
    }

    void draw_rect(int x, int y, int w, int h, unsigned char color) {
        draw_line(x,     y,     x+w-1, y,     color);
        draw_line(x,     y+h-1, x+w-1, y+h-1, color);
        draw_line(x,     y,     x,     y+h-1, color);
        draw_line(x+w-1, y,     x+w-1, y+h-1, color);
    }

    void fill_rect(int x, int y, int w, int h, unsigned char color) {
        for (int row = y; row < y + h; row++)
            for (int col = x; col < x + w; col++)
                set_pixel(col, row, color);
    }

    // Palette de couleurs standard VGA (index 0-15)
    static const unsigned char BLACK   = 0;
    static const unsigned char BLUE    = 1;
    static const unsigned char GREEN   = 2;
    static const unsigned char CYAN    = 3;
    static const unsigned char RED     = 4;
    static const unsigned char MAGENTA = 5;
    static const unsigned char BROWN   = 6;
    static const unsigned char WHITE   = 15;
    static const unsigned char YELLOW  = 14;
};
