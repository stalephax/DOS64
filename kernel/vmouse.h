#pragma once
#include "drivers/mouse.h"

class MouseCursor {
    unsigned short* VGA = (unsigned short*)0xB8000;
    int cur_x = 40, cur_y = 12;  // position actuelle
    unsigned short saved_cell = 0; // cellule sauvegardée sous le curseur

    void restore() {
        VGA[cur_y * 80 + cur_x] = saved_cell;
    }

    void draw() {
        saved_cell = VGA[cur_y * 80 + cur_x];
        // Inverser les couleurs de la cellule (swap fg/bg)
        unsigned char color = (saved_cell >> 8) & 0xFF;
        unsigned char fg    = color & 0x0F;
        unsigned char bg    = (color >> 4) & 0x0F;
        unsigned char inv   = (fg << 4) | bg;  // inverser fg et bg
        unsigned char ch    = saved_cell & 0xFF;
        VGA[cur_y * 80 + cur_x] = (unsigned short)ch | (inv << 8);
    }

public:
    void update(PS2Mouse* mouse) {
        int dx, dy;
        bool btn_left, btn_right;

        if (!mouse->poll(dx, dy, btn_left, btn_right)) return;

        // Effacer l'ancien curseur
        restore();

        // Mettre à jour la position
        cur_x = mouse->get_x();
        cur_y = mouse->get_y();

        // Dessiner le nouveau curseur
        draw();
    }

    int get_x() { return cur_x; }
    int get_y() { return cur_y; }
};