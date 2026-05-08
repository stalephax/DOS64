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
    void* port_ctx = nullptr;
    unsigned char (*port_in_cb)(unsigned short, void*) = nullptr;
    void (*port_out_cb)(unsigned short, unsigned char, void*) = nullptr;

    static const unsigned int RM_MEM_SIZE = 1024u * 1024u;

    unsigned int linear(unsigned short seg, unsigned short off) const {
        return ((unsigned int)seg << 4) + off;
    }
    FAT32* fs; 
    FAT32_File open_files[16]; // Table des handles (0-15) pas plus, pas moins

    
    void init_handles() {
        for(int i = 0; i < 16; i++) open_files[i].valid = false;
    }
    // décodeur RM

    // --- LECTURE/ÉCRITURE MÉMOIRE CORRIGÉES ---
    
    void write_mem8(unsigned char* mem, unsigned int phys_addr, unsigned char val) {
        // On redirige la VGA vers le Framebuffer
        if (phys_addr >= 0xA0000 && phys_addr <= 0xAFFFF) { 
            unsigned char* vga_mem = (unsigned char*)0xA0000;
            vga_mem[phys_addr - 0xA0000] = val;
        } 
        // SINON ON ÉCRIT DANS LA RAM (vital !)
        else if (phys_addr < RM_MEM_SIZE) {
            mem[phys_addr] = val;
        }
    }

    void write_mem16(unsigned char* mem, unsigned int phys_addr, unsigned short val) {
        write_mem8(mem, phys_addr, (unsigned char)(val & 0xFF));
        write_mem8(mem, phys_addr + 1, (unsigned char)(val >> 8));
    }

    unsigned short read_mem16(unsigned char* mem, unsigned int phys_addr) {
        if (phys_addr + 1 >= RM_MEM_SIZE) return 0;
        return (unsigned short)(mem[phys_addr] | (mem[phys_addr + 1] << 8));
    }

    // --- MOTEUR MOD R/M SÉCURISÉ ---

    unsigned int get_effective_address(RealModeRegs* r, unsigned char modrm, unsigned char* mem, unsigned short& ip_offset) {
        unsigned char mod = (modrm >> 6) & 0x03;
        unsigned char rm = modrm & 0x07;
        unsigned short offset = 0;
        unsigned short segment = r->ds; // DS par défaut

        // 1. Calcul de la base
        switch (rm) {
            case 0: offset = r->bx + r->si; break;
            case 1: offset = r->bx + r->di; break;
            case 2: offset = r->bp + r->si; segment = r->ss; break; // BP -> SS
            case 3: offset = r->bp + r->di; segment = r->ss; break; // BP -> SS
            case 4: offset = r->si; break;
            case 5: offset = r->di; break;
            case 6: 
                if (mod == 0) { // Adresse directe 16-bit
                    offset = (unsigned short)(mem[linear(r->cs, ip_offset)] | (mem[linear(r->cs, ip_offset + 1)] << 8));
                    ip_offset += 2; // On consomme 2 octets
                } else {
                    offset = r->bp; segment = r->ss;
                }
                break;
            case 7: offset = r->bx; break;
        }
        // 2. Ajout du déplacement
        if (mod == 1) { // disp8
            signed char disp8 = (signed char)mem[linear(r->cs, ip_offset)];
            offset = (unsigned short)(offset + disp8);
            ip_offset += 1; // On consomme 1 octet
        } else if (mod == 2) { // disp16
            unsigned short disp16 = (unsigned short)(mem[linear(r->cs, ip_offset)] | (mem[linear(r->cs, ip_offset + 1)] << 8));
            offset = (unsigned short)(offset + disp16);
            ip_offset += 2; // On consomme 2 octets
        }

        return linear(segment, offset);
    }
    int load_and_run(const char* filename) {

            MZHeader hdr;
            // fat->read(f, &hdr, sizeof(MZHeader));
            if (hdr.signature != 0x5A4D) return -1; // "MZ"

            // 3. Allouer la mémoire (PSP + Code + Heap)
            // Utilisation de ton HeapAllocator (memory.h)
            unsigned int code_size = (hdr.pages_in_file * 512) - (hdr.header_paragraphs * 16);
            void* program_buffer = heap->malloc(0x100 + code_size + (hdr.min_extra_paragraphs * 16));

            return 0; 
        }

    unsigned short read_rm16(RealModeRegs* r, unsigned char modrm, unsigned char* mem, unsigned short& ip_offset) {
        if (((modrm >> 6) & 0x03) == 3) { // Mode REGISTRE
            return *reg16_by_index(r, modrm & 0x07);
        } else { // Mode MÉMOIRE
            unsigned int addr = get_effective_address(r, modrm, mem, ip_offset);
            return read_mem16(mem, addr);
        }
    }

    void write_rm16(RealModeRegs* r, unsigned char modrm, unsigned char* mem, unsigned short& ip_offset, unsigned short val) {
        if (((modrm >> 6) & 0x03) == 3) { // Mode REGISTRE
            *reg16_by_index(r, modrm & 0x07) = val;
        } else { // Mode MÉMOIRE
            unsigned int addr = get_effective_address(r, modrm, mem, ip_offset);
            write_mem16(mem, addr, val);
        }
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
    // don't qualify a method name with it's namespace if it is in the object defined or i'll kill you
    void write_mem8(unsigned int phys_addr, unsigned char val) { // WTF ?
        // Si le programme écrit dans la zone VGA (Mode 13h)
        if (phys_addr >= 0xA0000 && phys_addr <= 0xAFFFF) { 
            // si cela ne marche pas je vais te tuer
            unsigned char* vga_mem = (unsigned char*)0xA0000;
            vga_mem[phys_addr - 0xA0000] = val;
        }
        else return; // échec
    }
public:
    /*
     Instructions à implémenter pour exécuter WORD.EXE : 
  0x00  ( 38602 fois) /
  0xFF  ( 18466 fois) 
  0x8B  ( 15715 fois) /
  0x46  ( 11157 fois) 
  0x01  (  9794 fois) 
  0x60  (  9077 fois) /
  0x16  (  8869 fois) 
  0x5E  (  8408 fois) 
  0x06  (  8146 fois) 
  0x03  (  8085 fois) /
  0x02  (  7853 fois) /
  0x7E  (  6840 fois) 
  0x0E  (  6335 fois) 
  0x0E  (  6335 fois) 
  0x50  (  6270 fois)
  0x15  (  6072 fois)
  0x89  (  6038 fois)
  0x14  (  6012 fois)
  0x08  (  5785 fois)
  0x04  (  5683 fois) /
  0x56  (  5671 fois)
  0x76  (  5549 fois)
  0x0A  (  5497 fois)
  0x75  (  5491 fois)
  0x13  (  5435 fois) /
  0x12  (  5204 fois)
  0x10  (  5145 fois)
  0x90  (  5062 fois)
  0xF7  (  4911 fois)
  0x80  (  4833 fois)
  0x74  (  4818 fois)
  0xDD  (  4791 fois)
  0x20  (  4767 fois)
  0x05  (  4705 fois) /
  0x0C  (  4659 fois)
  0x17  (  4659 fois)
  0x9A  (  4635 fois)
  0x7F  (  4628 fois)
  0x67  (  4578 fois)
  0x07  (  4448 fois)
  0x0B  (  4423 fois)
  0xB8  (  4221 fois)
  0x11  (  4084 fois)
  0x3E  (  3946 fois)
  0x83  (  3913 fois)
  0xC0  (  3910 fois)
  0xF8  (  3882 fois)
  0xFA  (  3836 fois)
  0x0F  (  3594 fois)
  0x2A  (  3441 fois)
  0xFE  (  3377 fois)
  0x5F  (  3340 fois)
  0x55  (  3290 fois)
  0x47  (  3228 fois)
  0x09  (  3099 fois)
    
    */
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
    void bind_host_ports(
        unsigned char (*in_func)(unsigned short, void*),
        void (*out_func)(unsigned short, unsigned char, void*),
        void* ctx
    ) 
    {
        this->port_in_cb  = in_func;
        this->port_out_cb = out_func;
        this->port_ctx    = ctx;
    }

    bool is_mz_exe(const unsigned char* data, unsigned int size) {
        if (!data || size < sizeof(MZHeader)) return false;
        return ((const MZHeader*)data)->signature == 0x5A4D;
    }
        // Dans mzexe.h, ajoutons une fonction pour préparer le PSP directement dans la mémoire émulée
    int prepare_dos_environment(unsigned char* mem, unsigned short psp_seg, const char* cmd_line) {
        unsigned int psp_base = psp_seg << 4;

        // 1. Signature de sortie (INT 20h) à l'offset 0
        mem[psp_base + 0] = 0xCD; 
        mem[psp_base + 1] = 0x20;

    // 2. Segment de fin de mémoire (souvent 0x9FFF pour 640Ko)
        unsigned short last_seg = 0x9FFF;
        mem[psp_base + 2] = (unsigned char)(last_seg & 0xFF);
        mem[psp_base + 3] = (unsigned char)(last_seg >> 8);

    // 3. Ligne de commande (Offset 0x80)
    // Format : [taille (1 octet)] [texte] [0x0D]
        unsigned char len = 0;
        if (cmd_line) {
            while (cmd_line[len] && len < 126) {
                mem[psp_base + 0x81 + len] = (unsigned char)cmd_line[len];
                len++;
            }
        }
    mem[psp_base + 0x80] = len;       // Longueur
    mem[psp_base + 0x81 + len] = 0x0D; // Terminateur DOS CR

    return 0;
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

       // Extrait de la logique de chargement mise à jour
        unsigned short psp_seg = 0x1000;
        unsigned short load_seg = psp_seg + 0x10; // Le code commence après les 256 octets du PSP

// Prépare le PSP dans la mémoire émulée
        prepare_dos_environment(mem, psp_seg, "ARGS_ICI"); // pas encore d'arguments placés, placeholder ARGS_ICI

// Charge l'image du programme après le PSP
        
        unsigned int load_linear = linear(load_seg, 0);
        unsigned int image_size = size - header_size;
        if (load_linear + image_size >= RM_MEM_SIZE) { heap->free(psp); heap->free(mem); return -4; }
        for (unsigned int i = 0; i < image_size; i++) {
            mem[linear(load_seg, 0) + i] = data[header_size + i];
        }
        //for (unsigned int i = 0; i < image_size; i++) mem[load_linear + i] = data[header_size + i]; ne recopiez pas l'incrémentation ou je vous tue

        unsigned int reloc_table = hdr->relocation_table_offset;
        for (unsigned int i = 0; i < hdr->relocation_count; i++) { // j'ai mal aux yeux
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
        unsigned char ah = (unsigned char)(r->ax >> 8); // int21 Interrupt
        switch (ah) {
            case 0x73: 
            case 0x00: halt = true; exit_code = 0; set_cf(r, false); return 0; // kill process
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
        // les instructions sont ICI
        switch (op) {
            // ne toucher à rien ici ou je vous égorge
            case 0x00: { // ADD r/m8, r8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src = get_reg8(r, (modrm >> 3) & 7);
                if (((modrm >> 6) & 3) == 3) {
                    unsigned char dst = get_reg8(r, modrm & 7);
                    unsigned short result = (unsigned short)dst + src;
                    set_reg8(r, modrm & 7, (unsigned char)result);
                    set_zf(r, ((unsigned char)result) == 0);
                    set_cf(r, result > 0xFF);
                } else {
                    unsigned int addr = get_effective_address(r, modrm, mem, ip_off);
                    unsigned char dst = mem[addr];
                    unsigned short result = (unsigned short)dst + src;
                    write_mem8(mem, addr, (unsigned char)result);
                    set_zf(r, ((unsigned char)result) == 0);
                    set_cf(r, result > 0xFF);
                }
                r->ip = ip_off;
                return 0;
            }
            case 0x01: { // ADD r/m16, r16
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short src = *reg16_by_index(r, (modrm >> 3) & 7);
                unsigned short dst = read_rm16(r, modrm, mem, ip_off);
                unsigned int result = (unsigned int)dst + src;
                ip_off = r->ip + 2;
                write_rm16(r, modrm, mem, ip_off, (unsigned short)result);
                set_zf(r, ((unsigned short)result) == 0);
                set_cf(r, result > 0xFFFF);
                r->ip = ip_off;
                return 0;
            }
            case 0x06: push16(mem, r, r->es); r->ip += 1; return 0;
            case 0x07: r->es = pop16(mem, r); r->ip += 1; return 0;
            case 0x08: { // OR r/m8, r8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src = get_reg8(r, (modrm >> 3) & 7);
                unsigned char res;
                if (((modrm >> 6) & 3) == 3) {
                    res = (unsigned char)(get_reg8(r, modrm & 7) | src);
                    set_reg8(r, modrm & 7, res);
                } else {
                    unsigned int addr = get_effective_address(r, modrm, mem, ip_off);
                    res = (unsigned char)(mem[addr] | src);
                    write_mem8(mem, addr, res);
                }
                set_zf(r, res == 0);
                set_cf(r, false);
                r->ip = ip_off;
                return 0;
            }

            case 0x09: { // OR r/m16, r16
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short src = *reg16_by_index(r, (modrm >> 3) & 7);
                unsigned short dst = read_rm16(r, modrm, mem, ip_off);
                unsigned short res = (unsigned short)(dst | src);
                ip_off = r->ip + 2;
                write_rm16(r, modrm, mem, ip_off, res);
                set_zf(r, res == 0); set_cf(r, false);
                r->ip = ip_off; return 0;
            }
            case 0x10: { // ADC r/m8, r8
                unsigned char modrm = mem[pc + 1]; unsigned short ip_off = r->ip + 2;
                unsigned char src = get_reg8(r, (modrm >> 3) & 7); unsigned char cf = (r->flags & 1) ? 1 : 0;
                if (((modrm >> 6) & 3) == 3) { unsigned char dst = get_reg8(r, modrm & 7); unsigned short res = (unsigned short)dst + src + cf; set_reg8(r, modrm & 7, (unsigned char)res); set_zf(r, ((unsigned char)res) == 0); set_cf(r, res > 0xFF); }
                else { unsigned int addr = get_effective_address(r, modrm, mem, ip_off); unsigned char dst = mem[addr]; unsigned short res = (unsigned short)dst + src + cf; write_mem8(mem, addr, (unsigned char)res); set_zf(r, ((unsigned char)res) == 0); set_cf(r, res > 0xFF); }
                r->ip = ip_off; return 0;
            }
            case 0x11: { // ADC r/m16, r16
                unsigned char modrm = mem[pc + 1]; unsigned short ip_off = r->ip + 2;
                unsigned short src = *reg16_by_index(r, (modrm >> 3) & 7); unsigned short dst = read_rm16(r, modrm, mem, ip_off); unsigned int res = (unsigned int)dst + src + ((r->flags & 1) ? 1 : 0);
                ip_off = r->ip + 2; write_rm16(r, modrm, mem, ip_off, (unsigned short)res); set_zf(r, ((unsigned short)res) == 0); set_cf(r, res > 0xFFFF); r->ip = ip_off; return 0;
            }
            case 0x12: { // ADC r8, r/m8
                unsigned char modrm = mem[pc + 1]; unsigned short ip_off = r->ip + 2;
                unsigned char src = (((modrm >> 6) & 3) == 3) ? get_reg8(r, modrm & 7) : mem[get_effective_address(r, modrm, mem, ip_off)];
                unsigned char* dst = nullptr; unsigned char d = get_reg8(r, (modrm >> 3) & 7); unsigned short res = (unsigned short)d + src + ((r->flags & 1) ? 1 : 0);
                (void)dst; set_reg8(r, (modrm >> 3) & 7, (unsigned char)res); set_zf(r, ((unsigned char)res) == 0); set_cf(r, res > 0xFF); r->ip = ip_off; return 0;
            }
            case 0x13: { // ADC r16, r/m16
                unsigned char modrm = mem[pc + 1]; unsigned short ip_off = r->ip + 2;
                unsigned short src = read_rm16(r, modrm, mem, ip_off); unsigned short* d = reg16_by_index(r, (modrm >> 3) & 7); unsigned int res = (unsigned int)(*d) + src + ((r->flags & 1) ? 1 : 0);
                *d = (unsigned short)res; set_zf(r, *d == 0); set_cf(r, res > 0xFFFF); r->ip = ip_off; return 0;
            }
            case 0x14: { // ADC AL, imm8
                unsigned short res = (unsigned short)(r->ax & 0xFF) + mem[pc + 1] + ((r->flags & 1) ? 1 : 0); set_reg8(r, 0, (unsigned char)res); set_zf(r, ((unsigned char)res) == 0); set_cf(r, res > 0xFF); r->ip += 2; return 0;
            }
            case 0x15: { // ADC AX, imm16
                unsigned short imm = read_mem16(mem, linear(r->cs, r->ip + 1)); unsigned int res = (unsigned int)r->ax + imm + ((r->flags & 1) ? 1 : 0); r->ax = (unsigned short)res; set_zf(r, r->ax == 0); set_cf(r, res > 0xFFFF); r->ip += 3; return 0;
            }
            case 0x0A: { // OR r8, r/m8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src = (((modrm >> 6) & 3) == 3) ? get_reg8(r, modrm & 7) : mem[get_effective_address(r, modrm, mem, ip_off)];
                unsigned char res = (unsigned char)(get_reg8(r, (modrm >> 3) & 7) | src);
                set_reg8(r, (modrm >> 3) & 7, res);
                set_zf(r, res == 0); set_cf(r, false); r->ip = ip_off; return 0;
            }
            case 0x0B: { // OR r16, r/m16
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short val = read_rm16(r, modrm, mem, ip_off);
                unsigned short* dst = reg16_by_index(r, (modrm >> 3) & 7);
                *dst = (unsigned short)(*dst | val);
                set_zf(r, *dst == 0); set_cf(r, false); r->ip = ip_off; return 0;
            }
            case 0x0C: { unsigned char res = (unsigned char)((r->ax & 0xFF) | mem[pc + 1]); set_reg8(r, 0, res); set_zf(r, res == 0); set_cf(r, false); r->ip += 2; return 0; }
            case 0x0E: push16(mem, r, r->cs); r->ip += 1; return 0;
            case 0x16: push16(mem, r, r->ss); r->ip += 1; return 0;
            case 0x17: r->ss = pop16(mem, r); r->ip += 1; return 0;
            case 0x20: { // AND r/m8, r8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src = get_reg8(r, (modrm >> 3) & 7);
                if (((modrm >> 6) & 3) == 3) {
                    unsigned char res = (unsigned char)(get_reg8(r, modrm & 7) & src);
                    set_reg8(r, modrm & 7, res);
                    set_zf(r, res == 0);
                } else {
                    unsigned int addr = get_effective_address(r, modrm, mem, ip_off);
                    unsigned char res = (unsigned char)(mem[addr] & src);
                    write_mem8(mem, addr, res);
                    set_zf(r, res == 0);
                }
                set_cf(r, false); r->ip = ip_off; return 0;
            }
            case 0x3D: { // OPEN FILE
                unsigned int p = linear(r->ds, r->dx);
                const char* filename = (const char*)&mem[p];
                int handle = -1;
                for(int i = 5; i < 16; i++) { // On commence à 5 (0-4 sont réservés par DOS)
                    if(!open_files[i].valid) { handle = i; break; }
                }

                if(handle == -1) { r->ax = 0x04; set_cf(r, true); return 0; }

                open_files[handle] = fs->open(filename);

                if(open_files[handle].valid) {
                    r->ax = (unsigned short)handle;
                    set_cf(r, false);
                } else {
                    r->ax = 0x02; // File not found
                    set_cf(r, true);
                }
                return 0;
            }
            case 0x02: { // ADD r8, r/m8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src;

                if (((modrm >> 6) & 3) == 3) {
                    src = get_reg8(r, modrm & 7);
                } else {
                    unsigned int addr = get_effective_address(r, modrm, mem, ip_off);
                    src = mem[addr];
                }

                unsigned char dst    = get_reg8(r, (modrm >> 3) & 7);
                unsigned short result = (unsigned short)dst + src;
                set_reg8(r, (modrm >> 3) & 7, (unsigned char)(result & 0xFF));
                set_zf(r, (result & 0xFF) == 0);
                set_cf(r, result > 0xFF);
                r->ip = ip_off;
                return 0;
            }
            case 0xC6: { // MOV r/m8, imm8
                unsigned char modrm  = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char imm;

                if (((modrm >> 6) & 3) == 3) {
                    imm = mem[linear(r->cs, ip_off)];
                    set_reg8(r, modrm & 7, imm);
                    r->ip = ip_off + 1;
                } else {
                    unsigned int addr = get_effective_address(r, modrm, mem, ip_off);
                    imm = mem[linear(r->cs, ip_off)];
                    write_mem8(mem, addr, imm);
                    r->ip = ip_off + 1;
                }
                return 0;
            }



            case 0x0D: { // OR AX, imm16
                unsigned short imm = read_mem16(mem, linear(r->cs, r->ip + 1));
                r->ax |= imm;
                set_zf(r, r->ax == 0);
                r->flags &= ~0x0001;
                r->ip += 3;
                return 0;
            }



            case 0x0F: // déjà présent — mais ajoute 0x07 et 0x1F si pas là

            case 0x1F: { // POP DS
                r->ds = read_mem16(mem, linear(r->ss, r->sp));
                r->sp += 2;
                r->ip += 1;
                return 0;
            }

            case 0x9A: { // CALL FAR imm16:imm16
                unsigned short new_ip = read_mem16(mem, linear(r->cs, r->ip + 1));
                unsigned short new_cs = read_mem16(mem, linear(r->cs, r->ip + 3));
                // Push CS puis IP de retour
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->cs);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), (unsigned short)(r->ip + 5));
                r->cs = new_cs;
                r->ip = new_ip;
                return 0;
            }
            case 0x03: { // ADD r16, r/m16
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short src = read_rm16(r, modrm, mem, ip_off);
                unsigned short* dst = reg16_by_index(r, (modrm >> 3) & 7);
                unsigned int result = (unsigned int)*dst + src;
                *dst = (unsigned short)(result & 0xFFFF);
                set_zf(r, *dst == 0);
                set_cf(r, result > 0xFFFF);
                r->ip = ip_off;
                return 0;
            }

            case 0x04: { // ADD AL, imm8  ← déjà présent mais vérifie
                unsigned char imm = mem[pc + 1];
                unsigned char al  = (unsigned char)(r->ax & 0xFF);
                unsigned short result = (unsigned short)al + imm;
                set_reg8(r, 0, (unsigned char)(result & 0xFF));
                set_zf(r, (result & 0xFF) == 0);
                set_cf(r, result > 0xFF);
                r->ip += 2;
                return 0;
            }

            case 0x05: { // ADD AX, imm16
                unsigned short imm = read_mem16(mem, linear(r->cs, r->ip + 1));
                unsigned int result = (unsigned int)r->ax + imm;
                r->ax = (unsigned short)(result & 0xFFFF);
                set_zf(r, r->ax == 0);
                set_cf(r, result > 0xFFFF);
                r->ip += 3;
                return 0;
            }
            case 0x3F: { // READ FROM FILE
                int h = r->bx;
                if (h < 0 || h >= 16 || !open_files[h].valid) { set_cf(r, true); return 0; }

                unsigned short count = r->cx;
                unsigned int buffer_addr = linear(r->ds, r->dx);

                // On utilise f.position pour savoir où on en est
                FAT32_File* f = &open_files[h];
                
                // Astuce : On ne lit pas plus que ce qu'il reste dans le fichier
                if (f->position + count > f->file_size) count = f->file_size - f->position;

                // ICI : Tu dois appeler une version de read_file qui gère l'offset !
                // Si ta fonction FAT32::read_file ne gère pas f->position, Prince ne chargera rien.
                // Dans ton case 0x3F (READ FILE) de handle_int21 :
                bool ok = false;
                if (fs->read_file(f, &mem[buffer_addr], count)) {
                    f->position += count; // C'est ici qu'on fait avancer le pointeur pour la prochaine lecture !
                    r->ax = count;
                    set_cf(r, false);
                    ok = true;
                }
                //[cite: 15]; c'est quoi cette pourriture?

                if (ok) {
                    f->position += count; // On avance le curseur
                    r->ax = count;
                    set_cf(r, false);
                } else {
                    set_cf(r, true);
                }
                return 0;
            }

            case 0x42: { // LSEEK
                int h = r->bx;
                if (h < 0 || h >= 16 || !open_files[h].valid) { set_cf(r, true); return 0; }

                unsigned int offset = (unsigned int)((r->cx << 16) | r->dx);
                unsigned char origin = (unsigned char)(r->ax & 0xFF);
                FAT32_File* f = &open_files[h];

                if (origin == 0)      f->position = offset;               // Depuis le début
                else if (origin == 1) f->position += (signed int)offset;  // Depuis position actuelle
                else if (origin == 2) f->position = f->file_size + (signed int)offset; // Depuis la fin

                // DOS demande de renvoyer la nouvelle position dans DX:AX
                r->ax = (unsigned short)(f->position & 0xFFFF);
                r->dx = (unsigned short)(f->position >> 16);
                set_cf(r, false);
                return 0;
            }
            // je ne sais pas pourquoi j'ai pas mis ça avant mais voila
            case 0x73: { // JAE / JNB / JNC rel8
                
                signed char rel = (signed char)mem[pc + 1];
                

                bool cf = (r->flags & 0x1) != 0;

                if (!cf) {
                    r->ip = (unsigned short)(r->ip + 2 + rel);
                } else {
                    r->ip += 2; // Sinon, on passe juste à l'instruction suivante
                }
                return 0;
            }
            case 0x8C: { // MOV r/m16, Sreg
                unsigned char modrm = mem[pc + 1];
                unsigned short next_ip = r->ip + 2; // On saute Opcode + ModR/M
                unsigned char reg_part = (modrm >> 3) & 0x07;
                unsigned short val = 0;

                switch (reg_part) {
                    case 0: val = r->es; break;
                    case 1: val = r->cs; break;
                    case 2: val = r->ss; break;
                    case 3: val = r->ds; break;
                    default: return -11;
                }

                // La magie opère : si write_rm16 lit une adresse, next_ip avancera tout seul
                write_rm16(r, modrm, mem, next_ip, val); 
                r->ip = next_ip; 
                return 0;
            }

            case 0x8E: { // MOV Sreg, r/m16
                unsigned char modrm = mem[pc + 1];
                unsigned short next_ip = r->ip + 2;
                unsigned char reg_part = (modrm >> 3) & 0x07;
                
                unsigned short val = read_rm16(r, modrm, mem, next_ip);

                switch (reg_part) {
                    case 0: r->es = val; break;
                    case 2: r->ss = val; break;
                    case 3: r->ds = val; break;
                    default: return -11;
                }
                r->ip = next_ip;
                return 0;
            }
            case 0x2E: { // CS segment override prefix — on l'ignore simplement
                r->ip += 1;
                return 0;
            }
            case 0x26: { // ES segment override
                r->ip += 1; return 0;
            }
            case 0x36: { // SS segment override
                r->ip += 1; return 0;
            }
            case 0x3E: { // DS segment override
                r->ip += 1; return 0;
            }
            case 0x64: { // FS segment override
                r->ip += 1; return 0;
            }
            case 0x65: { // GS segment override
                r->ip += 1; return 0;
            }
            case 0xF2: { // REPNZ prefix
                r->ip += 1; return 0;
            }
            case 0xF3: { // REP/REPZ prefix — à gérer proprement pour MOVSB/STOSB
                r->ip += 1; return 0;
            }
            case 0x8B: { // MOV r16, r/m16
                unsigned char modrm = mem[pc + 1];
                unsigned short next_ip = r->ip + 2;
                unsigned char dst = (modrm >> 3) & 0x07;
                
                unsigned short val = read_rm16(r, modrm, mem, next_ip);
                *reg16_by_index(r, dst) = val;
                
                r->ip = next_ip;
                return 0;
            }
            case 0x60: { // PUSHA — push tous les registres
                unsigned short old_sp = r->sp;
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->ax);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->cx);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->dx);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->bx);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), old_sp);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->bp);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->si);
                r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->di);
                r->ip += 1; return 0;
            }

            case 0x61: { // POPA — pop tous les registres
                r->di = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
                r->si = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
                r->bp = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
                r->sp += 2;  // skip SP sauvegardé
                r->bx = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
                r->dx = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
                r->cx = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
                r->ax = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
                r->ip += 1; return 0;
            }



            case 0x83: { // ADD/SUB/CMP r/m16, imm8 (signe-étendu)
                unsigned char modrm = mem[pc + 1];
                unsigned char subop = (modrm >> 3) & 7;
                unsigned short ip_off = r->ip + 2;
                unsigned short val = read_rm16(r, modrm, mem, ip_off);
                signed char imm = (signed char)mem[linear(r->cs, ip_off)];
                unsigned short result = val;
                switch (subop) {
                    case 0: result = val + (unsigned short)(signed short)imm; break; // ADD
                    case 1: result = val | (unsigned short)(signed short)imm; break; // OR
                    case 2: result = val + (unsigned short)(signed short)imm; break; // ADC (simplifié)
                    case 3: result = val - (unsigned short)(signed short)imm; break; // SBB (simplifié)
                    case 4: result = val & (unsigned short)(signed short)imm; break; // AND
                    case 5: result = val - (unsigned short)(signed short)imm; break; // SUB
                    case 6: result = val ^ (unsigned short)(signed short)imm; break; // XOR
                    case 7: // CMP — pas d'écriture
                        set_zf(r, val == (unsigned short)(signed short)imm);
                        set_cf(r, val < (unsigned short)(signed short)imm);
                        r->ip = ip_off + 1;
                        return 0;
                }
                write_rm16(r, modrm, mem, ip_off, result);
                set_zf(r, result == 0);
                set_cf(r, result > 0xFFFF);
                r->ip = ip_off + 1;
                return 0;
            }

            case 0x67: { // Address size prefix (32 bit) — on ignore
                r->ip += 1; return 0;
            }

            case 0xDD: { // FPU — coprocesseur flottant — on ignore
                // Pour Word 5.0, on peut ignorer les instructions FPU
                // et espérer qu'il n'en a pas besoin pour le rendu texte
                unsigned char modrm = mem[pc + 1];
                unsigned char mod = (modrm >> 6) & 3;
                if (mod == 3) { r->ip += 2; }      // forme registre = 2 octets
                else          { r->ip += 4; }      // forme mémoire = environ 4 octets
                return 0;
            }
            

            case 0x1E: { // PUSH DS
                r->sp -= 2;
                write_mem16(mem, linear(r->ss, r->sp), r->ds);
                r->ip += 1; return 0;
            }

            case 0x90: r->ip += 1; return 0;
            case 0x75: { 
                signed char rel = (signed char)mem[pc + 1];
                bool zf = (r->flags & 0x40) != 0;
                if (!zf) r->ip = (unsigned short)(r->ip + 2 + rel);
                else r->ip += 2;
                return 0;
            }
            /* code d'Imbécile heureux
            // --- NOUVEAU : MOV r16, r/m16 ---
            case 0x8B: {
                unsigned char modrm = mem[pc + 1];
                unsigned char dst = (modrm >> 3) & 7;
                unsigned char src = modrm & 7;
                if (((modrm >> 6) & 3) == 3) { // Mode registre à registre
                    *reg16_by_index(r, dst) = *reg16_by_index(r, src);
                    r->ip += 2;
                } else {
                    // Ici, il faudra plus tard gérer l'accès mémoire [SI], [BX+disp], etc.
                    trace_fault(r, op, modrm, -10); return -10;
                }
                return 0;
            }*/
           
            case 0xB4: { // MOV AH, imm8
                unsigned short ax = r->ax;
                ax = (unsigned short)((ax & 0x00FF) | ((unsigned short)mem[pc + 1] << 8));
                r->ax = ax;
                r->ip += 2;
                return 0;
            }
            case 0xA1: { // MOV AX, [imm16] — charger AX depuis adresse directe DS:imm16
                unsigned short addr = read_mem16(mem, linear(r->cs, r->ip + 1));
                r->ax = read_mem16(mem, linear(r->ds, addr));
                r->ip += 3;
                return 0;
            }

            case 0xA0: { // MOV AL, [imm16] — version 8 bit
                unsigned short addr = read_mem16(mem, linear(r->cs, r->ip + 1));
                set_reg8(r, 0, mem[linear(r->ds, addr)]);
                r->ip += 3;
                return 0;
            }

            case 0xA2: { // MOV [imm16], AL
                unsigned short addr = read_mem16(mem, linear(r->cs, r->ip + 1));
                write_mem8(mem, linear(r->ds, addr), (unsigned char)(r->ax & 0xFF));
                r->ip += 3;
                return 0;
            }

            // 0xA3 déjà vu — MOV [imm16], AX
            // assure-toi qu'il est correct :
            case 0xA3: {
                unsigned short addr = read_mem16(mem, linear(r->cs, r->ip + 1));
                write_mem16(mem, linear(r->ds, addr), r->ax);
                r->ip += 3;
                return 0;
            }
            case 0xEE: { // OUT DX, AL
                
                unsigned short port = r->dx;
                unsigned char val = (unsigned char)(r->ax & 0xFF);
                

                if (this->port_out_cb) {
                    this->port_out_cb(port, val, this->port_ctx);
                }
                r->ip += 1;
                return 0;
            }

            case 0xEF: { // OUT DX, AX (16-bit)
                unsigned short port = r->dx;
                unsigned short val = r->ax;
                if (this->port_out_cb) {
                    this->port_out_cb(port, (unsigned char)(val & 0xFF), this->port_ctx);
                    this->port_out_cb(port + 1, (unsigned char)(val >> 8), this->port_ctx);
                }
                r->ip += 1;
                return 0;
            }
            case 0xC3: r->ip = pop16(mem, r); return 0;
            case 0xE8: {  // Ecran de sortie normal
                signed short rel = (signed short)(mem[pc + 1] | (mem[pc + 2] << 8));
                unsigned short ret_ip = (unsigned short)(r->ip + 3);
                push16(mem, r, ret_ip);
                r->ip = (unsigned short)(ret_ip + rel);
                return 0;
            }
            
            case 0xF7: { // Group 3 (MUL, DIV, NOT, NEG 16-bit)
                unsigned char modrm = mem[pc + 1];
                unsigned char subop = (modrm >> 3) & 7;
                unsigned short* reg = reg16_by_index(r, modrm & 7);
                r->ip += 2;

                if (subop == 4) { // MUL AX, reg
                    unsigned int res = (unsigned int)r->ax * (*reg);
                    r->ax = (unsigned short)(res & 0xFFFF);
                    r->dx = (unsigned short)(res >> 16);
                } else if (subop == 6) { // DIV reg
                    if (*reg != 0) {
                        unsigned int num = (unsigned int)((r->dx << 16) | r->ax);
                        r->ax = (unsigned short)(num / (*reg));
                        r->dx = (unsigned short)(num % (*reg));
                    }
                }
                return 0;
            }

            case 0xA4: { // MOVSB (Copie de mémoire DS:SI vers ES:DI) - Très utilisé en vidéo
                unsigned int src = linear(r->ds, r->si);
                unsigned int dst = linear(r->es, r->di);
                mem[dst] = mem[src];
                // Gestion du Direction Flag (DF) pour savoir si on avance ou recule
                short inc = (r->flags & 0x0400) ? -1 : 1; 
                r->si += inc; r->di += inc;
                r->ip += 1;
                return 0;
            }

            case 0xAC: { // LODSB (Charge DS:SI dans AL)
                r->ax = (unsigned short)((r->ax & 0xFF00) | mem[linear(r->ds, r->si)]);
                short inc = (r->flags & 0x0400) ? -1 : 1;
                r->si += inc;
                r->ip += 1;
                return 0;
            }
            case 0xCD: { // Ecran de décès n°2
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
            /*case 0x8E: { // MOV Sreg, r/m16 (register only) sers à rien, déja là
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
            }*/
            case 0xFC: // CLD	Clear direction flag
            {
                // je sais pas si ça marche, qui s'en fout que ça rende le patron content?
                r->flags &= 0x0400;
                r->ip += 1;
                return 0;
            }
            case 0x30: { // XOR r/m8, r8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src = get_reg8(r, (modrm >> 3) & 7);
                if (((modrm >> 6) & 3) == 3) {
                    unsigned char result = get_reg8(r, modrm & 7) ^ src;
                    set_reg8(r, modrm & 7, result);
                    set_zf(r, result == 0);
                } else {
                    unsigned int addr = get_effective_address(r, modrm, mem, ip_off);
                    unsigned char result = mem[addr] ^ src;
                    write_mem8(mem, addr, result);
                    set_zf(r, result == 0);
                }
                r->flags &= ~0x0001;
                r->ip = ip_off;
                return 0;
            }

            case 0x31: { // XOR r/m16, r16
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short src = *reg16_by_index(r, (modrm >> 3) & 7);
                unsigned short val = read_rm16(r, modrm, mem, ip_off);
                unsigned short result = val ^ src;
                ip_off = r->ip + 2;
                write_rm16(r, modrm, mem, ip_off, result);
                set_zf(r, result == 0);
                r->flags &= ~0x0001;
                r->ip = ip_off;
                return 0;
            }
            case 0x32: { // XOR r8, r/m8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src;

                if (((modrm >> 6) & 3) == 3) {
                    src = get_reg8(r, modrm & 7);
                } else {
                    unsigned int addr = get_effective_address(r, modrm, mem, ip_off);
                    src = mem[addr];
                }

                unsigned char dst = get_reg8(r, (modrm >> 3) & 7);
                unsigned char result = dst ^ src;
                set_reg8(r, (modrm >> 3) & 7, result);
                set_zf(r, result == 0);
                r->flags &= ~0x0001;  // clear CF
                r->ip = ip_off;
                return 0;
            }

            case 0x33: { // XOR r16, r/m16
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short val = read_rm16(r, modrm, mem, ip_off);
                unsigned short* dst = reg16_by_index(r, (modrm >> 3) & 7);
                unsigned short result = *dst ^ val;
                *dst = result;
                set_zf(r, result == 0);
                r->flags &= ~0x0001;
                r->ip = ip_off;
                return 0;
            }

            case 0x34: { // XOR AL, imm8
                unsigned char imm = mem[pc + 1];
                unsigned char al = (unsigned char)(r->ax & 0xFF);
                al ^= imm;
                set_reg8(r, 0, al);
                set_zf(r, al == 0);
                r->flags &= ~0x0001;
                r->ip += 2;
                return 0;
            }

            case 0x35: { // XOR AX, imm16
                unsigned short imm = read_mem16(mem, linear(r->cs, r->ip + 1));
                r->ax ^= imm;
                set_zf(r, r->ax == 0);
                r->flags &= ~0x0001;
                r->ip += 3;
                return 0;
            }

            case 0x88: { // MOV r/m8, r8 (Écriture en mémoire ou registre)
                unsigned char modrm = mem[pc + 1];
                unsigned short current_ip = (unsigned short)(r->ip + 2);
                unsigned char src_val = get_reg8(r, (modrm >> 3) & 0x7);

                if (((modrm >> 6) & 0x3) == 3) { // Mode registre
                    set_reg8(r, modrm & 0x7, src_val);
                    r->ip = current_ip;
                } else { // Mode MÉMOIRE
                    unsigned int addr = get_effective_address(r, modrm, mem, current_ip);
                    mem[addr] = src_val;
                    r->ip = current_ip;
                }
                return 0;
            }
            case 0xF8: { // CLC — clear carry flag
                r->flags &= ~0x0001;
                r->ip += 1; return 0;
            }
            case 0xF9: { // STC — set carry flag
                r->flags |= 0x0001;
                r->ip += 1; return 0;
            }
            case 0xFA: { // CLI — disable interrupts (on ignore)
                r->ip += 1; return 0;
            }
            case 0xFB: { // STI — enable interrupts (on ignore)
                r->ip += 1; return 0;
            }
            case 0xFD: { // STD — set direction flag
                r->flags |= 0x0400;
                r->ip += 1; return 0;
            }
            case 0x98: { // CBW — convert byte to word (signe-étend AL dans AX)
                signed char al = (signed char)(r->ax & 0xFF);
                r->ax = (unsigned short)(signed short)al;
                r->ip += 1; return 0;
            }
            case 0x99: { // CWD — convert word to doubleword (AX → DX:AX)
                if (r->ax & 0x8000) r->dx = 0xFFFF;
                else r->dx = 0;
                r->ip += 1; return 0;
            }
            case 0x9C: { // PUSHF
                r->sp -= 2;
                write_mem16(mem, linear(r->ss, r->sp), r->flags);
                r->ip += 1; return 0;
            }
            case 0x9D: { // POPF
                r->flags = read_mem16(mem, linear(r->ss, r->sp));
                r->sp += 2;
                r->ip += 1; return 0;
            }
            case 0xD0: { // Shifts groupe 2 — r/m8, 1
                unsigned char modrm = mem[pc + 1];
                unsigned char subop = (modrm >> 3) & 7;
                unsigned char v = get_reg8(r, modrm & 7);
                switch (subop) {
                    case 4: v <<= 1; break;           // SHL
                    case 5: v >>= 1; break;           // SHR
                    case 7: v = (signed char)v >> 1; break; // SAR
                }
                set_reg8(r, modrm & 7, v);
                set_zf(r, v == 0);
                r->ip += 2; return 0;
            }
            case 0xD1: { // Shifts groupe 2 — r/m16, 1
                unsigned char modrm = mem[pc + 1];
                unsigned char subop = (modrm >> 3) & 7;
                unsigned short ip_off = r->ip + 2;
                unsigned short v = read_rm16(r, modrm, mem, ip_off);
                switch (subop) {
                    case 4: v <<= 1; break;
                    case 5: v >>= 1; break;
                    case 7: v = (signed short)v >> 1; break;
                }
                ip_off = r->ip + 2;
                write_rm16(r, modrm, mem, ip_off, v);
                set_zf(r, v == 0);
                r->ip = ip_off; return 0;
            }
            case 0xD3: { // Shifts groupe 2 — r/m16, CL
                unsigned char modrm = mem[pc + 1];
                unsigned char subop = (modrm >> 3) & 7;
                unsigned char cl = (unsigned char)(r->cx & 0xFF);
                unsigned short ip_off = r->ip + 2;
                unsigned short v = read_rm16(r, modrm, mem, ip_off);
                switch (subop) {
                    case 4: v <<= cl; break;
                    case 5: v >>= cl; break;
                    case 7: v = (signed short)v >> cl; break;
                }
                ip_off = r->ip + 2;
                write_rm16(r, modrm, mem, ip_off, v);
                set_zf(r, v == 0);
                r->ip = ip_off; return 0;
            }

            case 0x8A: { // MOV r8, r/m8 (Lecture depuis la mémoire ou registre)
                unsigned char modrm = mem[pc + 1];
                unsigned short current_ip = (unsigned short)(r->ip + 2);
                unsigned char val;

                if (((modrm >> 6) & 0x3) == 3) { // Mode registre
                    val = get_reg8(r, modrm & 0x7);
                } else { // Mode MÉMOIRE
                    unsigned int addr = get_effective_address(r, modrm, mem, current_ip);
                    val = mem[addr];
                }
                set_reg8(r, (modrm >> 3) & 0x7, val);
                r->ip = current_ip;
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
            case 0x28: { // SUB r/m8, r8
                unsigned char modrm = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned char src = get_reg8(r, (modrm >> 3) & 7);
                if (((modrm >> 6) & 3) == 3) {
                    unsigned char val    = get_reg8(r, modrm & 7);
                    unsigned char result = val - src;
                    set_reg8(r, modrm & 7, result);
                    set_zf(r, result == 0);
                    set_cf(r, val < src);
                } else {
                    unsigned int addr    = get_effective_address(r, modrm, mem, ip_off);
                    unsigned char val    = mem[addr];
                    unsigned char result = val - src;
                    write_mem8(mem, addr, result);
                    set_zf(r, result == 0);
                    set_cf(r, val < src);
                }
                r->ip = ip_off;
                return 0;
            }

            case 0x29: { // SUB r/m16, r16
                unsigned char modrm  = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short src   = *reg16_by_index(r, (modrm >> 3) & 7);
                unsigned short val   = read_rm16(r, modrm, mem, ip_off);
                unsigned short result = val - src;
                ip_off = r->ip + 2;
                write_rm16(r, modrm, mem, ip_off, result);
                set_zf(r, result == 0);
                set_cf(r, val < src);
                r->ip = ip_off;
                return 0;
            }

            case 0x2B: { // SUB r16, r/m16
                unsigned char modrm  = mem[pc + 1];
                unsigned short ip_off = r->ip + 2;
                unsigned short src   = read_rm16(r, modrm, mem, ip_off);
                unsigned short* dst  = reg16_by_index(r, (modrm >> 3) & 7);
                set_cf(r, *dst < src);
                *dst -= src;
                set_zf(r, *dst == 0);
                r->ip = ip_off;
                return 0;
            }

            case 0x2C: { // SUB AL, imm8
                unsigned char imm    = mem[pc + 1];
                unsigned char al     = (unsigned char)(r->ax & 0xFF);
                set_cf(r, al < imm);
                al -= imm;
                set_reg8(r, 0, al);
                set_zf(r, al == 0);
                r->ip += 2;
                return 0;
            }

            case 0x2D: { // SUB AX, imm16
                unsigned short imm = read_mem16(mem, linear(r->cs, r->ip + 1));
                set_cf(r, r->ax < imm);
                r->ax -= imm;
                set_zf(r, r->ax == 0);
                r->ip += 3;
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

            case 0x46: r->si += 1; set_zf(r, r->si == 0); r->ip += 1; return 0;
            case 0x47: r->di += 1; set_zf(r, r->di == 0); r->ip += 1; return 0;
            case 0x74: { signed char rel = (signed char)mem[pc + 1]; if (r->flags & 0x40) r->ip = (unsigned short)(r->ip + 2 + rel); else r->ip += 2; return 0; }
            case 0x7E: { signed char rel = (signed char)mem[pc + 1]; bool zf = (r->flags & 0x40) != 0; bool sf = (r->flags & 0x80) != 0; bool of = (r->flags & 0x800) != 0; if (zf || (sf != of)) r->ip = (unsigned short)(r->ip + 2 + rel); else r->ip += 2; return 0; }
            case 0x7F: { signed char rel = (signed char)mem[pc + 1]; bool zf = (r->flags & 0x40) != 0; bool sf = (r->flags & 0x80) != 0; bool of = (r->flags & 0x800) != 0; if (!zf && (sf == of)) r->ip = (unsigned short)(r->ip + 2 + rel); else r->ip += 2; return 0; }
            case 0x80: { // Group1 r/m8, imm8
                unsigned char modrm = mem[pc + 1]; unsigned char subop = (modrm >> 3) & 7; unsigned short ip_off = r->ip + 2; unsigned char imm = mem[linear(r->cs, ip_off)];
                unsigned char val = (((modrm >> 6) & 3) == 3) ? get_reg8(r, modrm & 7) : mem[get_effective_address(r, modrm, mem, ip_off)];
                unsigned char res = val;
                if (subop == 0) res = (unsigned char)(val + imm); else if (subop == 4) res = (unsigned char)(val & imm); else if (subop == 5 || subop == 7) res = (unsigned char)(val - imm); else if (subop == 1) res = (unsigned char)(val | imm); else if (subop == 6) res = (unsigned char)(val ^ imm);
                if (subop != 7) { if (((modrm >> 6) & 3) == 3) set_reg8(r, modrm & 7, res); else { unsigned short tmp = r->ip + 2; unsigned int addr = get_effective_address(r, modrm, mem, tmp); write_mem8(mem, addr, res); } }
                set_zf(r, res == 0); if (subop == 5 || subop == 7) set_cf(r, val < imm); else set_cf(r, false); r->ip = ip_off + 1; return 0;
            }
            case 0x89: { unsigned char modrm = mem[pc + 1]; unsigned short ip_off = r->ip + 2; unsigned short src = *reg16_by_index(r, (modrm >> 3) & 7); write_rm16(r, modrm, mem, ip_off, src); r->ip = ip_off; return 0; }
            case 0xFE: { // INC/DEC r/m8
                unsigned char modrm = mem[pc + 1]; unsigned char subop = (modrm >> 3) & 7; unsigned short ip_off = r->ip + 2; unsigned char v = (((modrm >> 6) & 3) == 3) ? get_reg8(r, modrm & 7) : mem[get_effective_address(r, modrm, mem, ip_off)];
                if (subop == 0) v += 1; else if (subop == 1) v -= 1; else { trace_fault(r, op, modrm, -10); return -10; }
                if (((modrm >> 6) & 3) == 3) set_reg8(r, modrm & 7, v); else { unsigned short tmp = r->ip + 2; unsigned int addr = get_effective_address(r, modrm, mem, tmp); write_mem8(mem, addr, v); }
                set_zf(r, v == 0); r->ip = ip_off; return 0;
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
