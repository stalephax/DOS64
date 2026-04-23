// DOS64 - GFX editor ELF userland demo
// ESC: quit | ENTER: new line | BACKSPACE: erase
#include "std.h"




static void draw_rect(int x, int y, int w, int h, unsigned char c) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            sys_gfx_pixel(xx, yy, c);
}

static void glyph5x7(char c, unsigned char out[7]) {
    for (int i = 0; i < 7; i++) out[i] = 0;
    if (c >= 'a' && c <= 'z') c = c - 32;
    switch (c) {
        case 'A': { unsigned char g[7]={14,17,17,31,17,17,17}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'B': { unsigned char g[7]={30,17,17,30,17,17,30}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'C': { unsigned char g[7]={14,17,16,16,16,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'D': { unsigned char g[7]={30,17,17,17,17,17,30}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'E': { unsigned char g[7]={31,16,16,30,16,16,31}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'F': { unsigned char g[7]={31,16,16,30,16,16,16}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'G': { unsigned char g[7]={14,17,16,23,17,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'H': { unsigned char g[7]={17,17,17,31,17,17,17}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'I': { unsigned char g[7]={14,4,4,4,4,4,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'J': { unsigned char g[7]={1,1,1,1,17,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'K': { unsigned char g[7]={17,18,20,24,20,18,17}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'L': { unsigned char g[7]={16,16,16,16,16,16,31}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'M': { unsigned char g[7]={17,27,21,21,17,17,17}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'N': { unsigned char g[7]={17,25,21,19,17,17,17}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'O': { unsigned char g[7]={14,17,17,17,17,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'P': { unsigned char g[7]={30,17,17,30,16,16,16}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'Q': { unsigned char g[7]={14,17,17,17,21,18,13}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'R': { unsigned char g[7]={30,17,17,30,20,18,17}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'S': { unsigned char g[7]={15,16,16,14,1,1,30}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'T': { unsigned char g[7]={31,4,4,4,4,4,4}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'U': { unsigned char g[7]={17,17,17,17,17,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'V': { unsigned char g[7]={17,17,17,17,17,10,4}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'W': { unsigned char g[7]={17,17,17,21,21,21,10}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'X': { unsigned char g[7]={17,17,10,4,10,17,17}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'Y': { unsigned char g[7]={17,17,10,4,4,4,4}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case 'Z': { unsigned char g[7]={31,1,2,4,8,16,31}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '0': { unsigned char g[7]={14,17,19,21,25,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '1': { unsigned char g[7]={4,12,4,4,4,4,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '2': { unsigned char g[7]={14,17,1,2,4,8,31}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '3': { unsigned char g[7]={30,1,1,14,1,1,30}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '4': { unsigned char g[7]={2,6,10,18,31,2,2}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '5': { unsigned char g[7]={31,16,16,30,1,1,30}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '6': { unsigned char g[7]={14,16,16,30,17,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '7': { unsigned char g[7]={31,1,2,4,8,8,8}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '8': { unsigned char g[7]={14,17,17,14,17,17,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '9': { unsigned char g[7]={14,17,17,15,1,1,14}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '.': { unsigned char g[7]={0,0,0,0,0,12,12}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case ',': { unsigned char g[7]={0,0,0,0,0,12,8}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '-': { unsigned char g[7]={0,0,0,31,0,0,0}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '_': { unsigned char g[7]={0,0,0,0,0,0,31}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case ':': { unsigned char g[7]={0,12,12,0,12,12,0}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '/': { unsigned char g[7]={1,2,4,8,16,0,0}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '(': { unsigned char g[7]={2,4,8,8,8,4,2}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case ')': { unsigned char g[7]={8,4,2,2,2,4,8}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '!': { unsigned char g[7]={4,4,4,4,4,0,4}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case '?': { unsigned char g[7]={14,17,1,2,4,0,4}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        case ' ': default: break;
    }
}

static void draw_char(int x, int y, char c, unsigned char fg, unsigned char bg) {
    unsigned char g[7];
    glyph5x7(c, g);
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            unsigned char on = (g[row] >> (4 - col)) & 1;
            sys_gfx_pixel(x + col, y + row, on ? fg : bg);
        }
    }
}

static void draw_text(int x, int y, const char* s, unsigned char fg, unsigned char bg) {
    for (int i = 0; s[i]; i++) draw_char(x + i * 6, y, s[i], fg, bg);
}

extern "C" int main() {
    static char text[20][52];
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 52; x++)
            text[y][x] = ' ';

    int cx = 0, cy = 0;
    sys_gfx_init();

    while (1) {
        sys_gfx_clear(1); // blue
        draw_rect(0, 0, 320, 12, 15); // title bar
        draw_rect(0, 12, 320, 188, 0); // text area
        draw_text(4, 2, "GEDIT.ELF - ESC QUIT", 1, 15);

        for (int y = 0; y < 20; y++)
            for (int x = 0; x < 52; x++)
                draw_char(4 + x * 6, 16 + y * 8, text[y][x], 2, 0);

        draw_rect(3 + cx * 6, 15 + cy * 8, 7, 9, 14); // cursor

        char c = sys_getchar();
        if (!c) continue;
        if (c == 27) break; // ESC
        if (c == '\n') { cx = 0; if (cy < 19) cy++; continue; }
        if (c == '\b') {
            if (cx > 0) cx--;
            text[cy][cx] = ' ';
            continue;
        }
        if (c >= 32 && c <= 126) {
            text[cy][cx] = c;
            if (cx < 51) cx++;
        }
    }

    suicide(0); //
    return 0;
}
