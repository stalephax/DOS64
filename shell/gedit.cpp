// DOS64 - GFX editor ELF userland demo
// ESC: quit | ENTER: new line | BACKSPACE: erase
#include "std.h"


extern "C" int main() {
    static char text[20][52];
    static char file_buf[20 * 53];
    const char* path = "C:/GEDIT.TXT";
    const char* status = "CTRL+S SAVE | CTRL+L LOAD | ESC QUIT";

    for (int y = 0; y < 20; y++)
        for (int x = 0; x < 52; x++)
            text[y][x] = ' ';

    int cx = 0, cy = 0;
    sys_gfx_init();

    while (1) {
        sys_gfx_clear(1); // blue
        draw_rect(0, 0, 320, 12, 15); // title bar
        draw_rect(0, 12, 320, 188, 0); // text area
        draw_text(4, 2, "GFX MODE EDITOR", 1, 15);
        draw_text(80, 2, status, 1, 15);

        for (int y = 0; y < 20; y++)
            for (int x = 0; x < 52; x++)
                draw_char(4 + x * 6, 16 + y * 8, text[y][x], 2, 0);

        draw_rect(3 + cx * 6, 15 + cy * 8, 7, 9, 14); // cursor

        char c = sys_getchar();
        if (!c) continue;
        if (c == 27) break; // ESC
        if (c == 19) { // Ctrl+S
            int p = 0;
            for (int y = 0; y < 20; y++) {
                for (int x = 0; x < 52; x++) file_buf[p++] = text[y][x];
                file_buf[p++] = '\n';
            }
            long long w = sys_fs_write(path, file_buf, (unsigned long long)p);
            status = (w >= 0) ? "SAVED C:/GEDIT.TXT" : "SAVE FAILED (need existing file & enough size)";
            continue;
        }
        if (c == 12) { // Ctrl+L
            long long r = sys_fs_read(path, file_buf, sizeof(file_buf));
            if (r > 0) {
                int p = 0;
                for (int y = 0; y < 20; y++) {
                    for (int x = 0; x < 52; x++) {
                        char ch = (p < r) ? file_buf[p++] : ' ';
                        if (ch == '\n' || ch == '\r') ch = ' ';
                        text[y][x] = ch;
                    }
                    while (p < r && file_buf[p] != '\n') p++;
                    if (p < r && file_buf[p] == '\n') p++;
                }
                status = "LOADED C:/GEDIT.TXT";
            } else {
                status = "LOAD FAILED (file not found?)";
            }
            continue;
        }
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

    sys_gfx_textmode();
    suicide(0);
    return 0;
}
