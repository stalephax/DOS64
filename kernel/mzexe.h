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

struct MZReloc {
    unsigned short offset;
    unsigned short segment;
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

    static const unsigned int RM_MEM_SIZE = 1024u * 1024u;

    unsigned int linear(unsigned short seg, unsigned short off) const {
        return ((unsigned int)seg << 4) + off;
    }

public:
    MZExeLoader(HeapAllocator* h) : heap(h) {}

    bool is_mz_exe(const unsigned char* data, unsigned int size) {
        if (!data || size < sizeof(MZHeader)) return false;
        const MZHeader* hdr = (const MZHeader*)data;
        return hdr->signature == 0x5A4D;
    }

    int build_real_mode_image(const unsigned char* data, unsigned int size,
                              unsigned char** out_mem,
                              RealModeRegs* out_regs,
                              DOSPSP** out_psp,
                              unsigned short* out_load_seg) {
        if (!is_mz_exe(data, size)) return -1;
        const MZHeader* hdr = (const MZHeader*)data;

        unsigned int header_size = (unsigned int)hdr->header_paragraphs * 16u;
        if (header_size >= size) return -2;

        unsigned char* mem = (unsigned char*)heap->malloc(RM_MEM_SIZE);
        if (!mem) return -101;
        for (unsigned int i = 0; i < RM_MEM_SIZE; i++) mem[i] = 0;

        DOSPSP* psp = (DOSPSP*)heap->malloc(sizeof(DOSPSP));
        if (!psp) {
            heap->free(mem);
            return -101;
        }
        psp->int20[0] = 0xCD;
        psp->int20[1] = 0x20;
        psp->last_segment = 0;
        for (int i = 0; i < 252; i++) psp->reserved[i] = 0;

        unsigned short psp_seg = 0x1000;
        unsigned short load_seg = psp_seg + 0x10;
        unsigned int load_linear = linear(load_seg, 0);
        unsigned int image_size = size - header_size;
        if (load_linear + image_size >= RM_MEM_SIZE) {
            heap->free(psp);
            heap->free(mem);
            return -4;
        }

        for (unsigned int i = 0; i < image_size; i++) {
            mem[load_linear + i] = data[header_size + i];
        }

        unsigned int reloc_table = hdr->relocation_table_offset;
        for (unsigned int i = 0; i < hdr->relocation_count; i++) {
            unsigned int entry_off = reloc_table + i * sizeof(MZReloc);
            if (entry_off + sizeof(MZReloc) > size) {
                heap->free(psp);
                heap->free(mem);
                return -5;
            }
            const MZReloc* r = (const MZReloc*)(data + entry_off);
            unsigned int patch_linear = linear(load_seg + r->segment, r->offset);
            if (patch_linear + 1 >= RM_MEM_SIZE) {
                heap->free(psp);
                heap->free(mem);
                return -6;
            }
            unsigned short oldv = (unsigned short)(mem[patch_linear] | (mem[patch_linear + 1] << 8));
            unsigned short newv = (unsigned short)(oldv + load_seg);
            mem[patch_linear] = (unsigned char)(newv & 0xFF);
            mem[patch_linear + 1] = (unsigned char)(newv >> 8);
        }

        if (out_regs) {
            out_regs->ax = out_regs->bx = out_regs->cx = out_regs->dx = 0;
            out_regs->si = out_regs->di = out_regs->bp = 0;
            out_regs->ip = hdr->initial_ip;
            out_regs->sp = hdr->initial_sp;
            out_regs->cs = (unsigned short)(load_seg + hdr->initial_cs);
            out_regs->ss = (unsigned short)(load_seg + hdr->initial_ss);
            out_regs->ds = psp_seg;
            out_regs->es = psp_seg;
            out_regs->fs = out_regs->gs = 0;
            out_regs->flags = 0x0200;
        }

        if (out_load_seg) *out_load_seg = load_seg;
        if (out_psp) *out_psp = psp;
        if (out_mem) *out_mem = mem;
        return 0;
    }



    void set_cf(RealModeRegs* r, bool on) {
        if (on) r->flags |= 0x0001;
        else r->flags &= (unsigned short)~0x0001;
    }

    int handle_int21(unsigned char* mem, RealModeRegs* r, bool& halt, int& exit_code) {
        unsigned char ah = (unsigned char)(r->ax >> 8);
        switch (ah) {
            case 0x00: // terminate
                halt = true; exit_code = 0; set_cf(r, false); return 0;
            case 0x09: { // print $ string at DS:DX
                unsigned int p = linear(r->ds, r->dx);
                unsigned int guard = 0;
                while (p < RM_MEM_SIZE && mem[p] != '$' && guard < 4096) {
                    guard++;
                    p++;
                }
                set_cf(r, false);
                return 0;
            }
            case 0x19: // get current drive -> A:
                r->ax = (unsigned short)((r->ax & 0xFF00) | 0x00);
                set_cf(r, false);
                return 0;
            case 0x1A: // set DTA
                set_cf(r, false);
                return 0;
            case 0x25: // set int vector
                set_cf(r, false);
                return 0;
            case 0x2A: // get date (dummy)
                r->cx = 2026;
                r->dx = (unsigned short)((5 << 8) | 1); // month/day
                r->ax = (unsigned short)((r->ax & 0xFF00) | 5); // friday
                set_cf(r, false);
                return 0;
            case 0x2C: // get time (dummy)
                r->cx = (unsigned short)((12 << 8) | 0);
                r->dx = 0;
                set_cf(r, false);
                return 0;
            case 0x30: // get DOS version
                r->ax = 0x0700; // 7.00
                r->bx = 0;
                set_cf(r, false);
                return 0;
            case 0x33: // ctrl-break/check
                set_cf(r, false);
                return 0;
            case 0x35: // get int vector
                r->es = 0;
                r->bx = 0;
                set_cf(r, false);
                return 0;
            case 0x3C:  // create file (stub fail)
            case 0x3D: // open file
            case 0x3E: // close file
            case 0x3F: // read file
            case 0x40: // write file
            case 0x41: // delete
            case 0x42: // lseek
            case 0x43: // attributes
            case 0x47: // get cwd
            case 0x4E: // find first
            case 0x4F: // find next
                r->ax = 0x0001; // invalid function/file (stub)
                set_cf(r, true);
                return 0;
            case 0x44: // ioctl
                r->dx = 0;
                set_cf(r, false);
                return 0;
            case 0x48: // allocate memory
                r->ax = 0x0008; // insufficient memory
                set_cf(r, true);
                return 0;
            case 0x49: // free memory
            case 0x4A: // resize memory
                set_cf(r, false);
                return 0;
            case 0x4C: // terminate with return code
                halt = true;
                exit_code = (int)(r->ax & 0xFF);
                set_cf(r, false);
                return 0;
            default:
                return -21;
        }
    }
    int emulate_step(unsigned char* mem, RealModeRegs* r, bool& halt, int& exit_code) {
        unsigned int pc = linear(r->cs, r->ip);
        unsigned char op = mem[pc];

        switch (op) {
            case 0x90: r->ip += 1; return 0; // NOP
            case 0xCD: { // INT imm8
                unsigned char intn = mem[pc + 1];
                r->ip += 2;
                if (intn == 0x20) { halt = true; exit_code = 0; return 0; }
                if (intn == 0x21) {
                    return handle_int21(mem, r, halt, exit_code);
                }
                if (intn == 0x10 || intn == 0x16 || intn == 0x1A) {
                    set_cf(r, false);
                    return 0;
                }
                return -20;
            }
            case 0xB8: // MOV AX, imm16
                r->ax = (unsigned short)(mem[pc + 1] | (mem[pc + 2] << 8));
                r->ip += 3;
                return 0;
            case 0xB4: // MOV AH, imm8
                r->ax = (unsigned short)((r->ax & 0x00FF) | (mem[pc + 1] << 8));
                r->ip += 2;
                return 0;
            case 0xEB: { // JMP short
                signed char rel = (signed char)mem[pc + 1];
                r->ip = (unsigned short)(r->ip + 2 + rel);
                return 0;
            }
            default:
                return -10; // unsupported opcode
        }
    }

    int execute_real_mode_stub(unsigned char* mem, RealModeRegs* regs, unsigned int max_steps, int* out_exit) {
        if (!mem || !regs) return -7;
        bool halt = false;
        int exit_code = 0;

        for (unsigned int i = 0; i < max_steps && !halt; i++) {
            int rc = emulate_step(mem, regs, halt, exit_code);
            if (rc != 0) return rc;
        }

        if (!halt) return -8;
        if (out_exit) *out_exit = exit_code;
        return 0;
    }
};
