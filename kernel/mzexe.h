#pragma once

#include "memory.h"
#include "standart.h"
#include "syscall.h"

#pragma pack(push, 1)
struct MZHeader {
    unsigned short signature;
    unsigned short bytes_in_last_page;
    unsigned short pages_in_file;
    unsigned short relocation_count;
    unsigned short header_paragraphs;
    unsigned short min_extra_paragraphs;
    unsigned short max_extra_paragraphs;
    unsigned short initial_ss;
    unsigned short initial_sp;
    unsigned short checksum;
    unsigned short initial_ip;
    unsigned short initial_cs;
    unsigned short relocation_table_offset;
    unsigned short overlay_number;
};
#pragma pack(pop)

struct RealModeRegs {
    unsigned short ax, bx, cx, dx;
    unsigned short si, di, bp;
    unsigned short cs, ds, es, fs, gs, ss;
    unsigned short ip, sp, flags;
};

struct DOSPSP {
    unsigned char int20[2];
    unsigned short last_segment;
    unsigned char reserved[252];
};

class MZExeLoader {
private:
    HeapAllocator* heap;

public:
    MZExeLoader(HeapAllocator* h) : heap(h) {}

    bool is_mz_exe(const unsigned char* data, unsigned int size) {
        if (!data || size < sizeof(MZHeader)) return false;
        const MZHeader* hdr = (const MZHeader*)data;
        return hdr->signature == 0x5A4D; // 'MZ'
    }

    int load_stub_environment(const unsigned char* data, unsigned int size, RealModeRegs* out_regs, DOSPSP** out_psp) {
        if (!is_mz_exe(data, size)) return -1;

        const MZHeader* hdr = (const MZHeader*)data;
        unsigned int image_offset = (unsigned int)hdr->header_paragraphs * 16u;
        if (image_offset >= size) return -2;

        DOSPSP* psp = (DOSPSP*)heap->malloc(sizeof(DOSPSP));
        if (!psp) return -101;

        psp->int20[0] = 0xCD;
        psp->int20[1] = 0x20;
        psp->last_segment = 0;
        for (int i = 0; i < 252; i++) psp->reserved[i] = 0;

        if (out_regs) {
            out_regs->ax = out_regs->bx = out_regs->cx = out_regs->dx = 0;
            out_regs->si = out_regs->di = out_regs->bp = 0;
            out_regs->ip = hdr->initial_ip;
            out_regs->sp = hdr->initial_sp;
            out_regs->cs = hdr->initial_cs;
            out_regs->ss = hdr->initial_ss;
            out_regs->ds = out_regs->es = out_regs->fs = out_regs->gs = 0;
            out_regs->flags = 0x0200;
        }

        if (out_psp) *out_psp = psp;
        return 0;
    }
};
