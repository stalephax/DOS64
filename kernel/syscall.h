#pragma once
#include "prompt.h"
#include "memory.h"
#include "vfs.h"
#include "standart.h"
#include "drivers/io.h"
#include "drivers/keyboard.h"
#include "drivers/video.h"
#include "pointer.h"
#include "fs/FAT32.h"
#include "panic.h"
// Numéros de syscall
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
#define SYS_GET_POINTER_X 21
#define SYS_GET_POINTER_Y 22
#define SYS_GET_POINTER_BUTTON 23
// Objets kernel (définis dans kernel64.cpp)
extern Terminal* term;
extern HeapAllocator* heap;
extern Keyboard* kbd;
extern VGAGraphics* vga;
extern FAT32* fs;
extern PathManager* pm; 
extern bool power;
extern MouseCursor* cursor;
extern "C" VGAGraphics* ensure_vga();
static FAT32* current_fs();

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

        // c'est juste un programme DOS, on va donner la possibilité de charger lui-même une table de traduction
        case SYS_GETCHAR: {
            char c =  kbd->getchar();
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

        case SYS_GFX_INIT:
            ensure_vga()->init();
            break;

        case SYS_GFX_CLR:
            ensure_vga()->clear((unsigned char)arg1);
            break;

        case SYS_GFX_PIX:
            ensure_vga()->set_pixel((int)arg1, (int)arg2, (unsigned char)arg3);
            break;

        case SYS_FS_READ: {
            if (!fs || !arg1 || !arg2) {
                asm volatile("mov $-1, %%rax" ::: "rax");
                break;
            }
            FAT32_File f = fs->open((const char*)arg1);
            if (!f.valid) {
                asm volatile("mov $-1, %%rax" ::: "rax");
                break;
            }
            unsigned int max_size = (unsigned int)arg3;
            unsigned int to_read = f.file_size < max_size ? f.file_size : max_size;
            if (!fs->read_file(&f, (void*)arg2, to_read)) {
                asm volatile("mov $-1, %%rax" ::: "rax");
                break;
            }
            asm volatile("mov %0, %%rax" :: "r"((unsigned long long)to_read) : "rax");
            break;
        }

        case SYS_FS_WRITE: {
            if (!fs || !arg1 || !arg2) {
                asm volatile("mov $-1, %%rax" ::: "rax");
                break;
            }
            unsigned int size = (unsigned int)arg3;
            bool ok = fs->overwrite_file((const char*)arg1, (const unsigned char*)arg2, size);
            if (!ok) {
                asm volatile("mov $-1, %%rax" ::: "rax");
                break;
            }
            asm volatile("mov %0, %%rax" :: "r"((unsigned long long)size) : "rax");
            break;
        }
        // Dans interrupt_handler_syscall(), ajouter :

        



case 15:  // SYS_FS_EXISTS
{
    const char* path = (const char*)arg1;
    char resolved[256];
    pm->resolve(path, resolved);
    FAT32* cur = current_fs();
    bool exists = cur && (cur->open(resolved).valid || cur->is_directory(resolved));
    asm volatile("mov %0, %%rax" :: "r"((long long)(exists ? 1 : 0)) : "rax");
    break;
}

case SYS_GET_POINTER_X: 
{
    MouseCursor *cs = cursor;
    int value = cs->get_x();
    asm volatile("mov %0, %%rax" :: "r"((long long)value) : "rax");
    break;
}
case SYS_GET_POINTER_Y: 
{
    MouseCursor *cs = cursor;
    int value = cs->get_y();
    asm volatile("mov %0, %%rax" :: "r"((long long)value) : "rax");
    break;
}
case 19:  // SYS_GETCWD
{
    char* buf = (char*)arg1;
    int   max = (int)arg2;
    if (buf && max > 0) {
        char prompt[64];
        pm->get_prompt(prompt);
        int i = 0;
        for (; prompt[i] && prompt[i] != '>' && i < max-1; i++)
            buf[i] = prompt[i];
        buf[i] = '\0';
    }
    break;
}

case 20:  // SYS_SETCWD
{
    const char* path = (const char*)arg1;
    long long result = pm->cd(path) ? 0 : -1;
    asm volatile("mov %0, %%rax" :: "r"(result) : "rax");
    break;
}

        

    }
}


extern "C" void interrupt_handler(unsigned long long int_num,
                                   unsigned long long error_code) {
    switch (int_num) {
        case 0:  fuckup("Division by zero",              error_code); break;
        case 6:  fuckup("Invalid instruction (#UD)",     error_code); break;
        case 8:  fuckup("Double Fault (#DF)",            error_code); break;
        case 13: fuckup("General Protection Fault (#GP)", error_code); break;
        case 14: {
            unsigned long long fault_addr;
            asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
            fuckup("Page Fault - bad address", fault_addr);
            break;
        }
        default: fuckup("Unknown interrupt", int_num); break;
    }
}
