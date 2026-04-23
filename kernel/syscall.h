#pragma once
#include "prompt.h"
#include "memory.h"
#include "standart.h"
#include "drivers/io.h"
#include "drivers/keyboard.h"
#include "panic.h"
// Numéros de syscall
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
#define SYS_TYPE    10
#define SYS_MKDIR   11
#define SYS_RENAME  12
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
#define GFX_WIDTH 800
#define GFX_HEIGHT 600
#define GFX_DRAWPIX 102400

// Objets kernel (définis dans kernel64.cpp)
extern Terminal* term;
extern HeapAllocator* heap;
extern Keyboard* kbd;
extern bool power;

// Contexte d'un programme en cours d'exécution
struct ProgramContext {
    bool    running = false;
    int     exit_code = 0;
};

static ProgramContext current_program;
extern "C" void handle_syscall(unsigned long long int_num, unsigned long long syscall_num);
// =============================================
// Handler principal — appelé depuis l'ASM
// =============================================
// Plus besoin de récupérer les registres manuellement !
extern "C" void interrupt_handler_syscall(
    unsigned long long syscall_num,
    unsigned long long arg1,
    unsigned long long arg2,
    unsigned long long arg3)
{
    switch (syscall_num) {
        case SYS_PRINT:
            if (arg1) term->print((const char*)arg1);
            break;

        case SYS_PRINTLN:
            if (arg1) term->println((const char*)arg1);
            break;

        case SYS_MALLOC: {
            void* ptr = heap->malloc(arg1);
            // Retourner la valeur dans RAX
            asm volatile("mov %0, %%rax" :: "r"(ptr) : "rax");
            break;
        }

        case SYS_FREE:
            if (arg1) heap->free((void*)arg1);
            break;

        case SYS_EXIT:
            current_program.running   = false;
            current_program.exit_code = (int)arg1;
            break;

        case SYS_GETCHAR: {
            char c = kbd->getchar();
            asm volatile("mov %0, %%rax" :: "r"((unsigned long long)c) : "rax");
            break;
        }

        case SYS_GETLINE: {
            char* buf = (char*)arg1;
            unsigned long long max = arg2;
            unsigned long long i = 0;
            while (i < max - 1) {
                char c = kbd->getchar();
                if (c == '\n') break;
                if (c == '\b' && i > 0) { i--; term->putchar('\b'); continue; }
                buf[i++] = c;
                term->putchar(c);
            }
            buf[i] = '\0';
            term->putchar('\n');
            break;
        }
    }
}

// Handler principal — exceptions CPU
extern "C" void interrupt_handler(unsigned long long int_num,
                                   unsigned long long error_code) {
    switch (int_num) {
        case 0:  kernel_panic("Division by zero",              error_code); break;
        case 6:  kernel_panic("Invalid instruction (#UD)",     error_code); break;
        case 8:  kernel_panic("Double Fault (#DF)",            error_code); break;
        case 13: kernel_panic("General Protection Fault (#GP)", error_code); break;
        case 14: {
            unsigned long long fault_addr;
            asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
            kernel_panic("Page Fault - bad address", fault_addr);
            break;
        }
        default: kernel_panic("Unknown interrupt", int_num); break;
    }
}