// ============================================================
// stdio.cpp — DOS64 Standard I/O Library
// Lecture/écriture fichiers, formatage, conversions
// ============================================================
#include "std.h"

// ============================================================
// Numéros de syscalls filesystem (à ajouter dans std.h aussi)
// ============================================================
#define SYS_FS_READ   11
#define SYS_FS_WRITE  12
#define SYS_FS_OPEN   13
#define SYS_FS_CLOSE  14
#define SYS_FS_EXISTS 15
#define SYS_FS_SIZE   16
#define SYS_FS_MKDIR  17
#define SYS_FS_DEL    18
#define SYS_GETCWD    19
#define SYS_SETCWD    20

// ============================================================
// IO standart library
// ============================================================

// Lire un fichier entier dans un buffer
// Retourne le nombre d'octets lus, ou -1 si erreur
long long sys_fs_read(const char* path, char* buf, unsigned long long max) {
    long long result;
    asm volatile(
        "mov $11, %%rax\n"
        "mov %1,  %%rdi\n"   // path
        "mov %2,  %%rsi\n"   // buffer
        "mov %3,  %%rdx\n"   // max size
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(path), "r"(buf), "r"(max)
        : "rax", "rdi", "rsi", "rdx");
    return result;
}

// Écrire un buffer dans un fichier
// Retourne le nombre d'octets écrits, ou -1 si erreur
long long sys_fs_write(const char* path, const char* buf, unsigned long long size) {
    long long result;
    asm volatile(
        "mov $12, %%rax\n"
        "mov %1,  %%rdi\n"   // path
        "mov %2,  %%rsi\n"   // buffer
        "mov %3,  %%rdx\n"   // size
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(path), "r"(buf), "r"(size)
        : "rax", "rdi", "rsi", "rdx");
    return result;
}

// Vérifier si un fichier existe
// Retourne 1 si existe, 0 sinon
int sys_fs_exists(const char* path) {
    long long result;
    asm volatile(
        "mov $15, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(path)
        : "rax", "rdi");
    return (int)result;
}

// Obtenir la taille d'un fichier
// Retourne la taille en octets, ou -1 si erreur
long long sys_fs_size(const char* path) {
    long long result;
    asm volatile(
        "mov $16, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(path)
        : "rax", "rdi");
    return result;
}

// Créer un répertoire
// Retourne 0 si succès, -1 si erreur
int sys_fs_mkdir(const char* path) {
    long long result;
    asm volatile(
        "mov $17, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(path)
        : "rax", "rdi");
    return (int)result;
}

// Supprimer un fichier
// Retourne 0 si succès, -1 si erreur
int sys_fs_del(const char* path) {
    long long result;
    asm volatile(
        "mov $18, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(path)
        : "rax", "rdi");
    return (int)result;
}

// Obtenir le répertoire courant
void sys_getcwd(char* buf, int max) {
    asm volatile(
        "mov $19, %%rax\n"
        "mov %0,  %%rdi\n"
        "mov %1,  %%rsi\n"
        "int $0x80\n"
        :: "r"(buf), "r"((long long)max)
        : "rax", "rdi", "rsi");
}

// Changer de répertoire
int sys_setcwd(const char* path) {
    long long result;
    asm volatile(
        "mov $20, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(path)
        : "rax", "rdi");
    return (int)result;
}

// ============================================================
// Formatage — print_int, print_hex, printf minimal
// ============================================================

void print_int(long long n) {
    char buf[32];
    itoa(n, buf);
    print(buf);
}

void print_hex(unsigned long long n) {
    char buf[20];
    htoa(n, buf);
    print(buf);
}

// printf minimal — supporte %s %d %x %c %%
void printf(const char* fmt, ...) {
    // Récupérer les arguments depuis la stack
    // On utilise une approche simple sans va_list (pas de libc)
    unsigned long long* args;
    asm volatile("lea 16(%%rbp), %0" : "=r"(args));
    int arg_idx = 0;

    char buf[512];
    int  out = 0;

    for (int i = 0; fmt[i] && out < 511; i++) {
        if (fmt[i] != '%') {
            buf[out++] = fmt[i];
            continue;
        }
        i++;
        switch (fmt[i]) {
            case 's': {
                const char* s = (const char*)args[arg_idx++];
                if (!s) s = "(null)";
                for (int j = 0; s[j] && out < 511; j++)
                    buf[out++] = s[j];
                break;
            }
            case 'd': {
                char tmp[32];
                itoa((long long)args[arg_idx++], tmp);
                for (int j = 0; tmp[j] && out < 511; j++)
                    buf[out++] = tmp[j];
                break;
            }
            case 'u': {
                char tmp[32];
                utoa((unsigned long long)args[arg_idx++], tmp);
                for (int j = 0; tmp[j] && out < 511; j++)
                    buf[out++] = tmp[j];
                break;
            }
            case 'x':
            case 'X': {
                char tmp[20];
                htoa((unsigned long long)args[arg_idx++], tmp);
                for (int j = 0; tmp[j] && out < 511; j++)
                    buf[out++] = tmp[j];
                break;
            }
            case 'c': {
                buf[out++] = (char)args[arg_idx++];
                break;
            }
            case '%': {
                buf[out++] = '%';
                break;
            }
            default:
                buf[out++] = '%';
                buf[out++] = fmt[i];
                break;
        }
    }
    buf[out] = '\0';
    print(buf);
}

// ============================================================
// Conversions — itoa, utoa, htoa, atoi
// ============================================================

void itoa(long long n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    bool neg = (n < 0);
    if (neg) n = -n;
    char tmp[32];
    int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    if (neg) tmp[i++] = '-';
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}

void utoa(unsigned long long n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[32];
    int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}

void htoa(unsigned long long n, char* buf) {
    const char* hex = "0123456789ABCDEF";
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++)
        buf[17 - i] = hex[(n >> (i * 4)) & 0xF];
    buf[18] = '\0';
}

long long atoi(const char* s) {
    long long result = 0;
    bool neg = false;
    while (*s == ' ') s++;
    if (*s == '-') { neg = true; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9')
        result = result * 10 + (*s++ - '0');
    return neg ? -result : result;
}

// ============================================================
// Math
// ============================================================

long long abs(long long x)              { return x < 0 ? -x : x; }
long long min(long long a, long long b) { return a < b ? a : b; }
long long max(long long a, long long b) { return a > b ? a : b; }

// ============================================================
// GFX helpers (ligne et rectangle — pas de syscall dédié,
// on les construit depuis gfx_pixel)
// ============================================================

void gfx_line(int x1, int y1, int x2, int y2, unsigned char color) {
    int dx = x2 - x1; if (dx < 0) dx = -dx;
    int dy = y2 - y1; if (dy < 0) dy = -dy;
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        gfx_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 <  dx) { err += dx; y1 += sy; }
    }
}

void gfx_rect(int x, int y, int w, int h, unsigned char color) {
    gfx_line(x,     y,     x+w-1, y,     color);
    gfx_line(x,     y+h-1, x+w-1, y+h-1, color);
    gfx_line(x,     y,     x,     y+h-1, color);
    gfx_line(x+w-1, y,     x+w-1, y+h-1, color);
}

void gfx_fill(int x, int y, int w, int h, unsigned char color) {
    for (int row = y; row < y + h; row++)
        for (int col = x; col < x + w; col++)
            gfx_pixel(col, row, color);
}