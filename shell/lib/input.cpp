#include "std.h"
// Souris basique I/O
struct Pointer {
    int x,y;
    int button;
};

static Pointer get_pointer() {
    Pointer* p;
    p->x = pointer_x();
    p->y = pointer_y();
    p->button = pointer_button();
}

static int pointer_x() {
    long long result;
    asm volatile(
        "mov $21, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(result)
        : "rax", "rdi");
    return (int)result;
}
static int pointer_y() {
    long long result;
    asm volatile(
        "mov $22, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(result)
        : "rax", "rdi");
    return (int)result;
}
static int pointer_button() { // PAS IMPLEMENTE :'(
    long long result;
    asm volatile(
        "mov $23, %%rax\n"
        "mov %1,  %%rdi\n"
        "int $0x80\n"
        "mov %%rax, %0\n"
        : "=r"(result)
        : "r"(result)
        : "rax", "rdi");
    return (int)result; // 0 pour clic gauche, 1 pour clic droit, etc
}