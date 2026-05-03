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

struct RMTraceState {
    unsigned short cs;
    unsigned short ip;
    unsigned char opcode;
    unsigned char modrm;
    int reason;
};

class MZExeLoader {
private:
    HeapAllocator* heap;
    RMTraceState last_trace;
    void (*host_putc)(char, void*);
    void* host_ctx;
    int (*host_bios_int)(unsigned char, RealModeRegs*, void*);
    void* host_bios_ctx;
    static const unsigned int RM_MEM_SIZE = 1024u * 1024u;

    unsigned int linear(unsigned short seg, unsigned short off) const {
        return ((unsigned int)seg << 4) + off;
    }

    unsigned short* reg16_by_index(RealModeRegs* r, unsigned char idx) {
        switch (idx & 7) {
            case 0: return &r->ax;
            case 1: return &r->cx;
            case 2: return &r->dx;
            case 3: return &r->bx;
            case 4: return &r->sp;
            case 5: return &r->bp;
            case 6: return &r->si;
            default: return &r->di;
        }
    }

    unsigned char get_reg8(RealModeRegs* r, unsigned char idx) {
        switch (idx & 7) {
            case 0: return (unsigned char)(r->ax & 0xFF);      // AL
            case 1: return (unsigned char)(r->cx & 0xFF);      // CL
            case 2: return (unsigned char)(r->dx & 0xFF);      // DL
            case 3: return (unsigned char)(r->bx & 0xFF);      // BL
            case 4: return (unsigned char)((r->ax >> 8) & 0xFF); // AH
            case 5: return (unsigned char)((r->cx >> 8) & 0xFF); // CH
            case 6: return (unsigned char)((r->dx >> 8) & 0xFF); // DH
            default: return (unsigned char)((r->bx >> 8) & 0xFF); // BH
        }
    }

    void set_reg8(RealModeRegs* r, unsigned char idx, unsigned char v) {
        switch (idx & 7) {
            case 0: r->ax = (unsigned short)((r->ax & 0xFF00) | v); break;
            case 1: r->cx = (unsigned short)((r->cx & 0xFF00) | v); break;
            case 2: r->dx = (unsigned short)((r->dx & 0xFF00) | v); break;
            case 3: r->bx = (unsigned short)((r->bx & 0xFF00) | v); break;
            case 4: r->ax = (unsigned short)((r->ax & 0x00FF) | ((unsigned short)v << 8)); break;
            case 5: r->cx = (unsigned short)((r->cx & 0x00FF) | ((unsigned short)v << 8)); break;
            case 6: r->dx = (unsigned short)((r->dx & 0x00FF) | ((unsigned short)v << 8)); break;
            case 7: r->bx = (unsigned short)((r->bx & 0x00FF) | ((unsigned short)v << 8)); break;
        }
    }

    void push16(unsigned char* mem, RealModeRegs* r, unsigned short v) {
        r->sp -= 2;
        unsigned int addr = linear(r->ss, r->sp);
        mem[addr] = (unsigned char)(v & 0xFF);
        mem[addr + 1] = (unsigned char)(v >> 8);
    }

    unsigned short pop16(unsigned char* mem, RealModeRegs* r) {
        unsigned int addr = linear(r->ss, r->sp);
        unsigned short v = (unsigned short)(mem[addr] | (mem[addr + 1] << 8));
        r->sp += 2;
        return v;
    }

public:
    MZExeLoader(HeapAllocator* h) : heap(h) {
        last_trace.cs = last_trace.ip = 0;
        last_trace.opcode = last_trace.modrm = 0;
        last_trace.reason = 0;
        host_putc = nullptr;
        host_ctx = nullptr;
        host_bios_int = nullptr;
        host_bios_ctx = nullptr;
    }

    void bind_host_console(void (*fn)(char, void*), void* ctx) {
        host_putc = fn;
        host_ctx = ctx;
    }

    void bind_host_bios(int (*fn)(unsigned char, RealModeRegs*, void*), void* ctx) {
        host_bios_int = fn;
        host_bios_ctx = ctx;
    }
    bool is_mz_exe(const unsigned char* data, unsigned int size) {
        if (!data || size < sizeof(MZHeader)) return false;
        return ((const MZHeader*)data)->signature == 0x5A4D;
    }

    int build_real_mode_image(const unsigned char* data, unsigned int size,
                              unsigned char** out_mem, RealModeRegs* out_regs,
                              DOSPSP** out_psp, unsigned short* out_load_seg) {
        if (!is_mz_exe(data, size)) return -1;
        const MZHeader* hdr = (const MZHeader*)data;
        unsigned int header_size = (unsigned int)hdr->header_paragraphs * 16u;
        if (header_size >= size) return -2;

        unsigned char* mem = (unsigned char*)heap->malloc(RM_MEM_SIZE);
        if (!mem) return -101;
        for (unsigned int i = 0; i < RM_MEM_SIZE; i++) mem[i] = 0;

        DOSPSP* psp = (DOSPSP*)heap->malloc(sizeof(DOSPSP));
        if (!psp) { heap->free(mem); return -101; }
        psp->int20[0] = 0xCD; psp->int20[1] = 0x20; psp->last_segment = 0;
        for (int i = 0; i < 252; i++) psp->reserved[i] = 0;

        unsigned short psp_seg = 0x1000;
        unsigned short load_seg = psp_seg + 0x10;
        unsigned int load_linear = linear(load_seg, 0);
        unsigned int image_size = size - header_size;
        if (load_linear + image_size >= RM_MEM_SIZE) { heap->free(psp); heap->free(mem); return -4; }

        for (unsigned int i = 0; i < image_size; i++) mem[load_linear + i] = data[header_size + i];

        unsigned int reloc_table = hdr->relocation_table_offset;
        for (unsigned int i = 0; i < hdr->relocation_count; i++) {
            unsigned int entry_off = reloc_table + i * sizeof(MZReloc);
            if (entry_off + sizeof(MZReloc) > size) { heap->free(psp); heap->free(mem); return -5; }
            const MZReloc* rr = (const MZReloc*)(data + entry_off);
            unsigned int patch_linear = linear((unsigned short)(load_seg + rr->segment), rr->offset);
            if (patch_linear + 1 >= RM_MEM_SIZE) { heap->free(psp); heap->free(mem); return -6; }
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
            out_regs->ds = psp_seg; out_regs->es = psp_seg;
            out_regs->fs = out_regs->gs = 0; out_regs->flags = 0x0200;
        }
        if (out_load_seg) *out_load_seg = load_seg;
        if (out_psp) *out_psp = psp;
        if (out_mem) *out_mem = mem;
        return 0;
    }

    void set_cf(RealModeRegs* r, bool on) { if (on) r->flags |= 1; else r->flags &= (unsigned short)~1; }
    void set_zf(RealModeRegs* r, bool on) { if (on) r->flags |= 0x40; else r->flags &= (unsigned short)~0x40; }

    int handle_int21(unsigned char* mem, RealModeRegs* r, bool& halt, int& exit_code) {
        (void)mem;
        unsigned char ah = (unsigned char)(r->ax >> 8);
        switch (ah) {
            case 0x00: halt = true; exit_code = 0; set_cf(r, false); return 0;
            case 0x02: { // write char in DL
                if (host_putc) host_putc((char)(r->dx & 0xFF), host_ctx);
                set_cf(r, false);
                return 0;
            }
            case 0x09: { // write DS:DX until $
                unsigned int p = linear(r->ds, r->dx);
                unsigned int guard = 0;
                while (p < RM_MEM_SIZE && guard < 8192) {
                    unsigned char c = mem[p++];
                    if (c == '$') break;
                    if (host_putc) host_putc((char)c, host_ctx);
                    guard++;
                }
                set_cf(r, false);
                return 0;
            }
            case 0x19: r->ax = (unsigned short)((r->ax & 0xFF00) | 0x00); set_cf(r, false); return 0;
            case 0x1A: case 0x25: case 0x33: case 0x44: case 0x49: case 0x4A: set_cf(r, false); return 0;
            case 0x2A: r->cx = 2026; r->dx = (unsigned short)((5 << 8) | 2); r->ax = (unsigned short)((r->ax & 0xFF00) | 6); set_cf(r, false); return 0;
            case 0x2C: r->cx = (unsigned short)((12 << 8) | 0); r->dx = 0; set_cf(r, false); return 0;
            case 0x30: r->ax = 0x0700; r->bx = 0; set_cf(r, false); return 0;
            case 0x35: r->es = 0; r->bx = 0; set_cf(r, false); return 0;
            case 0x4C: halt = true; exit_code = (int)(r->ax & 0xFF); set_cf(r, false); return 0;
            default: r->ax = 0x0001; set_cf(r, true); return 0;
        }
    }

    int emulate_step(unsigned char* mem, RealModeRegs* r, bool& halt, int& exit_code) {
        unsigned int pc = linear(r->cs, r->ip);
        unsigned char op = mem[pc];

        if (op >= 0x50 && op <= 0x57) { push16(mem, r, *reg16_by_index(r, op - 0x50)); r->ip += 1; return 0; }
        if (op >= 0x58 && op <= 0x5F) { *reg16_by_index(r, op - 0x58) = pop16(mem, r); r->ip += 1; return 0; }
        if (op >= 0xB8 && op <= 0xBF) {
            *reg16_by_index(r, op - 0xB8) = (unsigned short)(mem[pc + 1] | (mem[pc + 2] << 8));
            r->ip += 3; return 0;
        }

        switch (op) {
            case 0x90: r->ip += 1; return 0;

            case 0xB4: { // MOV AH, imm8
                unsigned short ax = r->ax;
                ax = (unsigned short)((ax & 0x00FF) | ((unsigned short)mem[pc + 1] << 8));
                r->ax = ax;
                r->ip += 2;
                return 0;
            }
            case 0xC3: r->ip = pop16(mem, r); return 0;
            case 0xE8: {
                signed short rel = (signed short)(mem[pc + 1] | (mem[pc + 2] << 8));
                unsigned short ret_ip = (unsigned short)(r->ip + 3);
                push16(mem, r, ret_ip);
                r->ip = (unsigned short)(ret_ip + rel);
                return 0;
            }
            case 0xCD: {
                unsigned char intn = mem[pc + 1]; r->ip += 2;
                if (intn == 0x20) { halt = true; exit_code = 0; return 0; }
                if (intn == 0x21) return handle_int21(mem, r, halt, exit_code);
                if (intn == 0x10 || intn == 0x16 || intn == 0x1A) {
                    if (host_bios_int) {
                        int brc = host_bios_int(intn, r, host_bios_ctx);
                        if (brc == 0) return 0;
                    }
                    set_cf(r, false);
                    return 0;
                }
                trace_fault(r, op, intn, -20);
                return -20;
            }
            case 0x8E: { // MOV Sreg, r/m16 (register only)
                unsigned char modrm = mem[pc + 1];
                unsigned char mod = (unsigned char)((modrm >> 6) & 0x3);
                unsigned char reg = (unsigned char)((modrm >> 3) & 0x7);
                unsigned char rm = (unsigned char)(modrm & 0x7);
                if (mod != 0x3) { trace_fault(r, op, modrm, -10); return -10; }
                unsigned short v = *reg16_by_index(r, rm);
                if (reg == 0) r->es = v;
                else if (reg == 2) r->ss = v;
                else if (reg == 3) r->ds = v;
                else { trace_fault(r, op, modrm, -10); return -10; }
                r->ip += 2;
                return 0;
            }
            case 0x88: { // MOV r/m8, r8 (register only)
                unsigned char modrm = mem[pc + 1];
                if (((modrm >> 6) & 0x3) != 0x3) { trace_fault(r, op, modrm, -10); return -10; }
                unsigned char src = (unsigned char)((modrm >> 3) & 0x7);
                unsigned char dst = (unsigned char)(modrm & 0x7);
                set_reg8(r, dst, get_reg8(r, src));
                r->ip += 2;
                return 0;
            }
            case 0x24: { // AND AL, imm8
                unsigned char al = (unsigned char)(r->ax & 0xFF);
                al &= mem[pc + 1];
                set_reg8(r, 0, al);
                set_zf(r, al == 0);
                r->ip += 2;
                return 0;
            }
            case 0x3C: { // CMP AL, imm8
                unsigned char al = (unsigned char)(r->ax & 0xFF);
                unsigned char imm = mem[pc + 1];
                set_zf(r, al == imm);
                set_cf(r, al < imm);
                r->ip += 2;
                return 0;
            }
            case 0x76: { // JBE rel8
                signed char rel = (signed char)mem[pc + 1];
                bool cf = (r->flags & 0x1) != 0;
                bool zf = (r->flags & 0x40) != 0;
                if (cf || zf) r->ip = (unsigned short)(r->ip + 2 + rel);
                else r->ip += 2;
                return 0;
            }
            case 0x04: { // ADD AL, imm8
                unsigned char al = (unsigned char)(r->ax & 0xFF);
                al = (unsigned char)(al + mem[pc + 1]);
                set_reg8(r, 0, al);
                set_zf(r, al == 0);
                r->ip += 2;
                return 0;
            }
            case 0xEB: { signed char rel = (signed char)mem[pc + 1]; r->ip = (unsigned short)(r->ip + 2 + rel); return 0; }
            case 0xC0: { // shifts group2 imm8 - only SHR r/m8,imm8 register form
                unsigned char modrm = mem[pc + 1];
                unsigned char imm = mem[pc + 2];
                unsigned char mod = (unsigned char)((modrm >> 6) & 0x3);
                unsigned char subop = (unsigned char)((modrm >> 3) & 0x7);
                unsigned char rm = (unsigned char)(modrm & 0x7);
                if (mod != 0x3 || subop != 5) { trace_fault(r, op, modrm, -10); return -10; }
                unsigned char v = get_reg8(r, rm);
                v = (unsigned char)(v >> imm);
                set_reg8(r, rm, v);
                set_zf(r, v == 0);
                r->ip += 3;
                return 0;
            }
            default: trace_fault(r, op, 0, -10); return -10;
        }
    }


    void trace_fault(RealModeRegs* r, unsigned char op, unsigned char modrm, int reason) {
        last_trace.cs = r->cs;
        last_trace.ip = r->ip;
        last_trace.opcode = op;
        last_trace.modrm = modrm;
        last_trace.reason = reason;
    }

    RMTraceState get_last_trace() const { return last_trace; }

    int execute_real_mode_stub(unsigned char* mem, RealModeRegs* regs, unsigned int max_steps, int* out_exit) {
        if (!mem || !regs) return -7;
        bool halt = false; int exit_code = 0;
        last_trace.cs = regs->cs;
        last_trace.ip = regs->ip;
        last_trace.opcode = 0;
        last_trace.modrm = 0;
        last_trace.reason = 0;
        for (unsigned int i = 0; i < max_steps && !halt; i++) {
            int rc = emulate_step(mem, regs, halt, exit_code);
            if (rc != 0) return rc;
        }
        if (!halt) return -8;
        if (out_exit) *out_exit = exit_code;
        return 0;
    }
};
