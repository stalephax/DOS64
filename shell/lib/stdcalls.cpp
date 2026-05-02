#include "std.h"

void print(const char* s) {
    asm volatile(
        "mov $0, %%rax\nmov %0, %%rdi\nint $0x80"
        :: "r"(s) : "rax", "rdi");
}

void println(const char* s) {
    asm volatile(
        "mov $1, %%rax\nmov %0, %%rdi\nint $0x80"
        :: "r"(s) : "rax", "rdi");
}

void* malloc(unsigned long long size) {
    void* ptr;
    asm volatile(
        "mov $2, %%rax\nmov %1, %%rdi\nint $0x80\nmov %%rax, %0"
        : "=r"(ptr) : "r"(size) : "rax", "rdi");
    return ptr;
}

void free(void* ptr) {
    asm volatile(
        "mov $3, %%rax\nmov %0, %%rdi\nint $0x80"
        :: "r"(ptr) : "rax", "rdi");
}

void exit(int code) {
    asm volatile(
        "mov $4, %%rax\nmov %0, %%rdi\nint $0x80"
        :: "r"((long long)code) : "rax", "rdi");
    while(true) {}
}

char getchar() {
    unsigned long long c;
    asm volatile(
        "mov $5, %%rax\nint $0x80\nmov %%rax, %0"
        : "=r"(c) :: "rax");
    return (char)c;
}

void getline(char* buf, int max) {
    asm volatile(
        "mov $6, %%rax\nmov %0, %%rdi\nmov %1, %%rsi\nint $0x80"
        :: "r"(buf), "r"((long long)max) : "rax", "rdi", "rsi");
}

void gfx_init() {
    asm volatile("mov $7, %%rax\nint $0x80" ::: "rax");
}

void gfx_clear(unsigned char color) {
    asm volatile(
        "mov $8, %%rax\nmov %0, %%rdi\nint $0x80"
        :: "r"((unsigned long long)color) : "rax", "rdi");
}

void gfx_pixel(int x, int y, unsigned char color) {
    asm volatile(
        "mov $9, %%rax\nmov %0, %%rdi\nmov %1, %%rsi\nmov %2, %%rdx\nint $0x80"
        ::
          "r"((unsigned long long)x),
          "r"((unsigned long long)y),
          "r"((unsigned long long)color)
        : "rax", "rdi", "rsi", "rdx");
}

void gfx_exit() {
    asm volatile("mov $12, %%rax\nint $0x80" ::: "rax"); // the system calls should ne be switched or the system will fuckup
}