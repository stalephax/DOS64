#pragma once
#include "drivers/io.h"
#include "drivers/keyboard.h"

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
        } else if (c == '\r') {
            col = 0;
        } else if (c == '\b') {
            if (col > 0) {
                col--;
                VGA[row * WIDTH + col] = (current_color << 8) | ' ';
            }
        } else {
            VGA[row * WIDTH + col] = (unsigned short)c | (current_color << 8);
            col++;
            if (col >= WIDTH) { col = 0; //row++; celui qui remet ça je le tue
                }
        }
        
        if (row >= HEIGHT) scroll();
        update_cursor();
    }

    void print(const char* str) {
        for (int i = 0; str[i]; i++) putchar(str[i]);
    }
    void print_fail() {println("Operation failed miserably. ");}
    void print_fuccess() {println("Operation failed successfully. ");}
    void print_success() {println("Operation done successfull !");}

    void println(const char* str) {
        print(str);
        putchar('\n');
    }

    void clear() {
        for (int i = 0; i < WIDTH * HEIGHT; i++)
            VGA[i] = (current_color << 8) | ' ';
        row = col = 0;
    }    
    void setcol(int c) {col = c;}
    // met à jour le curseur VGA 
    void update_cursor() {
        unsigned short pos = row * WIDTH + col;
        outb(0x3D4, 0x0F);
        outb(0x3D5, pos & 0xFF);
        outb(0x3D4, 0x0E);
        outb(0x3D5, (pos >> 8) & 0xFF);
    }
};

struct PromptLine {
    char buf[256];
    int len;
    int cursor;
    char saved[256];
    int saved_len;

    void clear() {
        for (int i = 0; i < 256; i++) { buf[i] = 0; saved[i] = 0; }
        len = 0;
        cursor = 0;
        saved_len = 0;
    }

    void save() {
        for (int i = 0; i <= len; i++) saved[i] = buf[i];
        buf[len] = '\0';
        saved_len = len;
    }

    void restore() {
        for (int i = 0; i <= saved_len; i++) buf[i] = saved[i];
        len = saved_len;
        cursor = len;
    }

    void set(const char* s) {
        len = 0;
        while (s[len] && len < 255) { buf[len] = s[len]; len++; }
        //buf[len] = '\0';
        cursor = len;
    }

    void insert(char c) {
        if (len >= 255) return;
        for (int i = len; i > cursor; --i) buf[i] = buf[i - 1];
        buf[cursor++] = c;
        len++;
        //buf[len] = '\0';
    }

    void backspace() {
        if (cursor <= 0) return;
        for (int i = cursor - 1; i < len - 1; i++) buf[i] = buf[i + 1];
        len--;
        cursor--;
        //buf[len] = '\0';
    }

    void del() {
        if (cursor >= len) return;
        for (int i = cursor; i < len - 1; i++) buf[i] = buf[i + 1];
        len--;
//        buf[len] = '\0';
    }
};

class PromptSession {
    Terminal* term;
    PromptLine line;
    int hist_pos = 0;

    void redraw(const char* prompt_str) {
        for (int i = 0; i < 80; i++) term->putchar(' ');
        term->putchar('\r'); 
        term->print(prompt_str);
        for (int i = 0; i < line.len; i++) term->putchar(line.buf[i]);
        int back = line.len - line.cursor;
        for (int i = 0; i < back; i++) term->putchar('\b');
    }

public:
    PromptSession(Terminal* t) : term(t) { line.clear(); }

    void begin(const char* prompt_str) {
        line.clear();
        hist_pos = 0;
        term->set_color(0x0A);
        term->print(prompt_str);
        term->set_color(0x0F);
    }

    bool feed_key(
        char c,
        const char* prompt_str,
        const char* (*history_get_cb)(int),
        char* out_line
    ) {
        if (c == '\n') {
            line.buf[line.len] = '\0';
            term->putchar('\n');
            for (int i = 0; i <= line.len; i++) out_line[i] = line.buf[i];
            return true;
        } else if (c == '\b') {
            line.backspace();
            redraw(prompt_str);
        } else if (c == (char)KEY_DEL) {
            line.del();
            redraw(prompt_str);
        } else if (c == (char)KEY_LEFT) {
            if (line.cursor > 0) { line.cursor--; term->putchar('\b'); }
        } else if (c == (char)KEY_RIGHT) {
            if (line.cursor < line.len) { term->putchar(line.buf[line.cursor]); line.cursor++; }
        } else if (c == (char)KEY_HOME) {
            while (line.cursor > 0) { term->putchar('\b'); line.cursor--; }
        } else if (c == (char)KEY_END) {
            while (line.cursor < line.len) { term->putchar(line.buf[line.cursor]); line.cursor++; }
        } else if (c == (char)KEY_UP) {
            if (hist_pos == 0) line.save();
            hist_pos++;
            const char* h = history_get_cb(hist_pos);
            if (h) line.set(h);
            else hist_pos--;
            redraw(prompt_str);
        } else if (c == (char)KEY_DOWN) {
            if (hist_pos <= 0) return false;
            hist_pos--;
            if (hist_pos == 0) line.restore();
            else {
                const char* h = history_get_cb(hist_pos);
                if (h) line.set(h);
            }
            redraw(prompt_str);
        } else if (c >= 32 && c <= 126) {
            line.insert(c);
            redraw(prompt_str);
        }
        return false;
    }
};

