#pragma once
#include "drivers/io.h"

class Terminal {
    int row = 0;
    int col = 0;
    static const int WIDTH  = 80;
    static const int HEIGHT = 25;
    unsigned short* const VGA = (unsigned short*)0xB8000;
    unsigned char current_color = 0x0F;

    void scroll() {
        // Décaler toutes les lignes d'une vers le haut
        for (int r = 0; r < HEIGHT - 1; r++)
            for (int c = 0; c < WIDTH; c++)
                VGA[r * WIDTH + c] = VGA[(r+1) * WIDTH + c];
        // Effacer la dernière ligne
        for (int c = 0; c < WIDTH; c++)
            VGA[(HEIGHT-1) * WIDTH + c] = (current_color << 8) | ' ';
        row = HEIGHT - 1;
    }



public:
    void set_color(unsigned char fg, unsigned char bg = 0x0) {
        current_color = (bg << 4) | fg;
    }

    void putchar(char c) {
        if (c == '\n') {
            col = 0;
            row++;
        } else if (c == '\b') {
            if (col > 0) {
                col--;
                VGA[row * WIDTH + col] = (current_color << 8) | ' ';
            }
        } else {
            VGA[row * WIDTH + col] = (unsigned short)c | (current_color << 8);
            col++;
            if (col >= WIDTH) { col = 0; row++; }
        }
        if (row >= HEIGHT) scroll();
        update_cursor();
    }

    void print(const char* str) {
        for (int i = 0; str[i]; i++) putchar(str[i]);
    }
    


    void println(const char* str) {
        print(str);
        putchar('\n');
    }

    void clear() {
        for (int i = 0; i < WIDTH * HEIGHT; i++)
            VGA[i] = (current_color << 8) | ' ';
        row = col = 0;
    }    

    // met à jour le curseur VGA 
    void update_cursor() {
        unsigned short pos = row * WIDTH + col;
        outb(0x3D4, 0x0F);
        outb(0x3D5, pos & 0xFF);
        outb(0x3D4, 0x0E);
        outb(0x3D5, (pos >> 8) & 0xFF);
    }
};

