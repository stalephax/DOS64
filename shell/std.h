// Programme test — syscalls DOS64
#pragma once
#include "../kernel/standart.h"
#include "../kernel/memory.h"
#define SYS_PRINT   0
#define SYS_PRINTLN 1
#define SYS_MALLOC  2
#define SYS_FREE    3
#define SYS_EXIT    4
#define SYS_GETCHAR 5
#define SYS_GETLINE 6
#define SYS_RUN     7
#define SYS_DIR     8
#define SYS_CD      9
#define SYS_DEL     13
#define SYS_POWEROFF 14
#define SYS_REBOOT   15
#define SYS_GETCWD   16
#define SYS_LS       17
#define SYS_OPEN     18
#define SYS_CLOSE    19
#define SYS_READ     20
#define SYS_WRITE    21
#define SYS_SEEK     22
#define SYS_TELL     23
#define SYS_FILESIZE 24
#define SYS_GFXMODE  25
#define SYS_GFX_INIT 7
#define SYS_GFX_CLEAR 8
#define SYS_GFX_PIXEL 9
#define SYS_GFX_INIT 7
#define SYS_GFX_CLEAR 8
#define SYS_GFX_PIXEL 9
#define SYS_FS_READ 10
#define SYS_FS_WRITE 11
#define SYS_GFX_TEXTMODE 12


static inline void sys_gfx_pixel(int x, int y, unsigned char color);


static inline void sys_gfx_textmode() {
    asm volatile("mov $12, %%rax\nint $0x80\n" ::: "rax");
}

static void draw_rect(int x, int y, int w, int h, unsigned char c) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            sys_gfx_pixel(xx, yy, c);
}



// RS Sérif
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
        case ' ': { unsigned char g[7]={0,0,0,0,0,0,0}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        default: { unsigned char g[7]={14,17,1,2,4,0,4}; for(int i=0;i<7;i++) out[i]=g[i]; } break;
        break;
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
static void write(const char* s) {
    asm volatile(
        "mov $0, %%rax\n"    // SYS_PRINT
        "mov %0, %%rdi\n"    // argument = string
        "int $0x80\n"
        :: "r"(s) : "rax", "rdi"
    );
}

static inline void sys_gfx_exit() {
    asm volatile("mov $10, %%rax\nint $0x80\n" ::: "rax");
}

static inline char sys_getchar() {
    unsigned long long out;
    asm volatile(
        "mov $5, %%rax\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(out)
        :
        : "rax"
    );
    return (char)out;
}

static inline void sys_gfx_init() {
    asm volatile("mov $7, %%rax\nint $0x80\n" ::: "rax");
}
static void* sys_malloc(unsigned long long size) {
    void* result;
    asm volatile(
        "mov $2, %%rax\n"
        "mov %1, %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(size)
        : "rax", "rdi"
    );
    return result;
}
static inline void sys_gfx_clear(unsigned char color) {
    asm volatile(
        "mov $8, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"((unsigned long long)color) : "rax", "rdi"
    );
}

static inline void sys_gfx_pixel(int x, int y, unsigned char color) {
    asm volatile(
        "mov $9, %%rax\n"
        "mov %0, %%rdi\n"
        "mov %1, %%rsi\n"
        "mov %2, %%rdx\n"
        "int $0x80\n"
        ::
          "r"((unsigned long long)x),
          "r"((unsigned long long)y),
          "r"((unsigned long long)color)
        : "rax", "rdi", "rsi", "rdx"
    );
}


static void writeLn(const char* s) {
    asm volatile(
        "mov $1, %%rax\n"    // SYS_PRINTLN
        "mov %0, %%rdi\n"    // argument = string
        "int $0x80\n"
        :: "r"(s) : "rax", "rdi"
    );
}

static void suicide(int code) {
    asm volatile(
        "mov $4, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"((long long)code) : "rax", "rdi"
    );
}

static void alloc(long size) {
    asm volatile(
        "mov $2, %%rax\n" // SYS_MALLOC
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"(size) : "rax", "rdi"
    );
}


static void free(long size) {
    asm volatile(
        "mov $3, %%rax\n" // SYS_FREE
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"(size) : "rax", "rdi"
    );

}
class Allocator {
public:
    void* malloc(unsigned long long size) { return sys_malloc(size); }
    void  free(void* p) { /* syscall SYS_FREE */ }
};

