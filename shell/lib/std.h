#pragma once

// ============================================================
// DOS64 Standard Library — std.h
// ============================================================

// --- Numéros de syscalls ---
#define SYS_PRINT     0
#define SYS_PRINTLN   1
#define SYS_MALLOC    2
#define SYS_FREE      3
#define SYS_EXIT      4
#define SYS_GETCHAR   5
#define SYS_GETLINE   6
#define SYS_GFX_INIT  7
#define SYS_GFX_CLR   8
#define SYS_GFX_PIX   9
#define SYS_GFX_EXIT  10
#define SYS_FS_READ   11
#define SYS_FS_WRITE  12
#define SYS_FS_EXISTS 15
#define SYS_FS_SIZE   16
#define SYS_FS_MKDIR  17
#define SYS_FS_DEL    18
#define SYS_GETCWD    19
#define SYS_SETCWD    20

// --- Terminal ---
void print(const char* s);
void println(const char* s);
void print_int(long long n);
void print_hex(unsigned long long n);
void printf(const char* fmt, ...);
char getchar();
void getline(char* buf, int max);
void kms(int code);

// --- Mémoire ---
void* malloc(unsigned long long size);
void  free(void* ptr);

// --- Strings ---
int    strlen(const char* s);
int    strcmp(const char* a, const char* b);
int    strncmp(const char* a, const char* b, int n);
char*  strcpy(char* dst, const char* src);
char*  strcat(char* dst, const char* src);
char*  strchr(const char* s, char c);
void*  memset(void* ptr, int val, unsigned long long size);
void*  memcpy(void* dst, const void* src, unsigned long long size);

// --- Conversion ---
void      itoa(long long n, char* buf);
void      utoa(unsigned long long n, char* buf);
void      htoa(unsigned long long n, char* buf);
long long atoi(const char* s);

// --- Math ---
long long abs(long long x);
long long min(long long a, long long b);
long long max(long long a, long long b);

// --- Filesystem ---
long long sys_fs_read(const char* path, char* buf, unsigned long long max);
long long sys_fs_write(const char* path, const char* buf, unsigned long long size);
int       sys_fs_exists(const char* path);
long long sys_fs_size(const char* path);
int       sys_fs_mkdir(const char* path);
int       sys_fs_del(const char* path);
void      sys_getcwd(char* buf, int max);
int       sys_setcwd(const char* path);

// --- GFX ---
void gfx_init();
void gfx_clear(unsigned char color);
void gfx_pixel(int x, int y, unsigned char color);
void gfx_exit();
void gfx_line(int x1, int y1, int x2, int y2, unsigned char color);
void gfx_rect(int x, int y, int w, int h, unsigned char color);
void gfx_fill(int x, int y, int w, int h, unsigned char color);