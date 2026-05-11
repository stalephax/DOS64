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
    // Tous les programmes quittent ou freeze en place, fais chier 
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
        unsigned short original_ip = r->ip;  // Sauvegarder l'IP original

        

        if (op >= 0x50 && op <= 0x57) { push16(mem, r, *reg16_by_index(r, op - 0x50)); r->ip += 1; return 0; }
        if (op >= 0x58 && op <= 0x5F) { *reg16_by_index(r, op - 0x58) = pop16(mem, r); r->ip += 1; return 0; }
        if (op >= 0xB8 && op <= 0xBF) {
            *reg16_by_index(r, op - 0xB8) = (unsigned short)(mem[pc + 1] | (mem[pc + 2] << 8));
            r->ip += 3; return 0;
        }
        // toutes les instructions sont ICI
        // ============================================================
        // EMULATEUR x86 16bit — switch(op) nettoyé
        // C'est vraiment NTVDM mais intégré
        // ============================================================
        switch (op) {
        // ============================================================
        // NOP / HALT
        // ============================================================
        case 0x90: r->ip += 1; return 0;  // NOP
        case 0xF4: halt = true; return 0; // HLT
        
        // ============================================================
        // Préfixes — tous ignorés (compatibilité basique)
        // ============================================================
        case 0x26: case 0x2E: case 0x36: case 0x3E:
        case 0x64: case 0x65: case 0x66: case 0x67:
        case 0xF2: case 0xF3:
            r->ip += 1; return 0;
        
        // ============================================================
        // FLAGS
        // ============================================================
        case 0xF5: r->flags ^= 0x0001;   r->ip += 1; return 0; // CMC
        case 0xF8: r->flags &= ~0x0001;  r->ip += 1; return 0; // CLC
        case 0xF9: r->flags |= 0x0001;   r->ip += 1; return 0; // STC
        case 0xFA: r->ip += 1; return 0;                        // CLI (ignoré)
        case 0xFB: r->ip += 1; return 0;                        // STI (ignoré)
        case 0xFC: r->flags &= ~0x0400;  r->ip += 1; return 0; // CLD
        case 0xFD: r->flags |= 0x0400;   r->ip += 1; return 0; // STD
        
        case 0x9C: // PUSHF
            r->sp -= 2; write_mem16(mem, linear(r->ss, r->sp), r->flags);
            r->ip += 1; return 0;
        case 0x9D: // POPF
            r->flags = read_mem16(mem, linear(r->ss, r->sp)); r->sp += 2;
            r->ip += 1; return 0;
        case 0x9E: // SAHF
            r->flags = (r->flags & 0xFF00) | ((r->ax >> 8) & 0xFF);
            r->ip += 1; return 0;
        case 0x9F: // LAHF
            r->ax = (r->ax & 0x00FF) | ((r->flags & 0xFF) << 8);
            r->ip += 1; return 0;
        
        // ============================================================
        // PUSH/POP registres 16 bit
        // ============================================================
        case 0x50: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->ax); r->ip+=1; return 0;
        case 0x51: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->cx); r->ip+=1; return 0;
        case 0x52: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->dx); r->ip+=1; return 0;
        case 0x53: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->bx); r->ip+=1; return 0;
        case 0x54: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->sp); r->ip+=1; return 0;
        case 0x55: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->bp); r->ip+=1; return 0;
        case 0x56: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->si); r->ip+=1; return 0;
        case 0x57: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->di); r->ip+=1; return 0;
        
        case 0x58: r->ax=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2; r->ip+=1; return 0;
        case 0x59: r->cx=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2; r->ip+=1; return 0;
        case 0x5A: r->dx=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2; r->ip+=1; return 0;
        case 0x5B: r->bx=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2; r->ip+=1; return 0;
        case 0x5C: r->sp=read_mem16(mem,linear(r->ss,r->sp));            r->ip+=1; return 0;
        case 0x5D: r->bp=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2; r->ip+=1; return 0;
        case 0x5E: r->si=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2; r->ip+=1; return 0;
        case 0x5F: r->di=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2; r->ip+=1; return 0;
        
        // PUSHA / POPA
        case 0x60: {
            unsigned short old_sp = r->sp;
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->ax);
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->cx);
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->dx);
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->bx);
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),old_sp);
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->bp);
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->si);
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->di);
            r->ip+=1; return 0;
        }
        case 0x61: {
            r->di=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;
            r->si=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;
            r->bp=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;
            r->sp+=2; // skip SP sauvegardé
            r->bx=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;
            r->dx=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;
            r->cx=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;
            r->ax=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;
            r->ip+=1; return 0;
        }
        
        // PUSH imm16
        case 0x68: {
            unsigned short imm = read_mem16(mem, linear(r->cs, r->ip+1));
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),imm);
            r->ip+=3; return 0;
        }
        // PUSH imm8 (signe-étendu)
        case 0x6A: {
            signed char imm = (signed char)mem[pc+1];
            r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),(unsigned short)(signed short)imm);
            r->ip+=2; return 0;
        }
        case 0x06: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->es); r->ip+=1; return 0;
        case 0x07: r->es=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;  r->ip+=1; return 0;
        case 0x0E: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->cs); r->ip+=1; return 0;
        case 0x16: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->ss); r->ip+=1; return 0;
        case 0x17: r->ss=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;  r->ip+=1; return 0;
        case 0x1E: r->sp-=2; write_mem16(mem,linear(r->ss,r->sp),r->ds); r->ip+=1; return 0;
        case 0x1F: r->ds=read_mem16(mem,linear(r->ss,r->sp)); r->sp+=2;  r->ip+=1; return 0;
        case 0x40: case 0x41: case 0x42: case 0x43:
        case 0x44: case 0x45: case 0x46: case 0x47: {
            unsigned short* reg = reg16_by_index(r, op-0x40);
            (*reg)++; set_zf(r, *reg==0); r->ip+=1; return 0;
        }
        case 0x48: case 0x49: case 0x4A: case 0x4B:
        case 0x4C: case 0x4D: case 0x4E: case 0x4F: {
            unsigned short* reg = reg16_by_index(r, op-0x48);
            (*reg)--; set_zf(r, *reg==0); r->ip+=1; return 0;
        }
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7:
            set_reg8(r, op-0xB0, mem[pc+1]); r->ip+=2; return 0;
        
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF:
            *reg16_by_index(r, op-0xB8) = read_mem16(mem, linear(r->cs,r->ip+1));
            r->ip+=3; return 0;
        case 0x91: case 0x92: case 0x93:
        case 0x94: case 0x95: case 0x96: case 0x97: {
            unsigned short* reg = reg16_by_index(r, op-0x90);
            unsigned short tmp = r->ax; r->ax = *reg; *reg = tmp;
            r->ip+=1; return 0;
        }
        case 0x88: { // MOV r/m8, r8
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned char src = get_reg8(r, (modrm>>3)&7);
            if (((modrm>>6)&3)==3) set_reg8(r, modrm&7, src);
            else write_mem8(mem, get_effective_address(r,modrm,mem,ip_off), src);
            r->ip = ip_off; return 0;
        }
        case 0x89: { // MOV r/m16, r16
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned short src = *reg16_by_index(r, (modrm>>3)&7);
            write_rm16(r, modrm, mem, ip_off, src);
            r->ip = ip_off; return 0;
        }
        case 0x8A: { // MOV r8, r/m8
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned char val = (((modrm>>6)&3)==3)
                ? get_reg8(r, modrm&7)
                : mem[get_effective_address(r, modrm, mem, ip_off)];
            set_reg8(r, (modrm>>3)&7, val);
            r->ip = ip_off; return 0;
        }
        case 0x8B: { // MOV r16, r/m16
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            *reg16_by_index(r, (modrm>>3)&7) = read_rm16(r, modrm, mem, ip_off);
            r->ip = ip_off; return 0;
        }
        case 0x8C: { // MOV r/m16, Sreg
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned short val = 0;
            switch ((modrm>>3)&7) {
                case 0: val=r->es; break; case 1: val=r->cs; break;
                case 2: val=r->ss; break; case 3: val=r->ds; break;
                case 4: val=r->fs; break; case 5: val=r->gs; break;
            }
            write_rm16(r, modrm, mem, ip_off, val);
            r->ip = ip_off; return 0;
        }
        case 0x8E: { // MOV Sreg, r/m16
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned short val = read_rm16(r, modrm, mem, ip_off);
            switch ((modrm>>3)&7) {
                case 0: r->es=val; break; case 2: r->ss=val; break;
                case 3: r->ds=val; break; case 4: r->fs=val; break;
                case 5: r->gs=val; break;
            }
            r->ip = ip_off; return 0;
        }
        case 0xC6: { // MOV r/m8, imm8
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned char imm = mem[linear(r->cs, ip_off)];
            if (((modrm>>6)&3)==3) set_reg8(r, modrm&7, imm);
            else write_mem8(mem, get_effective_address(r,modrm,mem,ip_off), imm);
            r->ip = ip_off+1; return 0;
        }
        case 0xC7: { // MOV r/m16, imm16
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned short imm = read_mem16(mem, linear(r->cs, ip_off));
            write_rm16(r, modrm, mem, ip_off, imm);
            r->ip = ip_off+2; return 0;
        }
        
        // MOV AX/AL, [addr] et [addr], AX/AL
        case 0xA0: { unsigned short a=read_mem16(mem,linear(r->cs,r->ip+1)); set_reg8(r,0,mem[linear(r->ds,a)]); r->ip+=3; return 0; }
        case 0xA1: { unsigned short a=read_mem16(mem,linear(r->cs,r->ip+1)); r->ax=read_mem16(mem,linear(r->ds,a)); r->ip+=3; return 0; }
        case 0xA2: { unsigned short a=read_mem16(mem,linear(r->cs,r->ip+1)); write_mem8(mem,linear(r->ds,a),(unsigned char)(r->ax&0xFF)); r->ip+=3; return 0; }
        case 0xA3: { unsigned short a=read_mem16(mem,linear(r->cs,r->ip+1)); write_mem16(mem,linear(r->ds,a),r->ax); r->ip+=3; return 0; }
        case 0x8D: { // LEA r16, m
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            *reg16_by_index(r,(modrm>>3)&7) = (unsigned short)(get_effective_address(r,modrm,mem,ip_off)&0xFFFF);
            r->ip = ip_off; return 0;
        }
        case 0x86: { // XCHG r8, r/m8
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned char tmp = get_reg8(r,(modrm>>3)&7);
            if (((modrm>>6)&3)==3) { set_reg8(r,(modrm>>3)&7,get_reg8(r,modrm&7)); set_reg8(r,modrm&7,tmp); }
            else { unsigned int addr=get_effective_address(r,modrm,mem,ip_off); set_reg8(r,(modrm>>3)&7,mem[addr]); write_mem8(mem,addr,tmp); }
            r->ip = ip_off; return 0;
        }
        case 0x87: { // XCHG r16, r/m16
            unsigned char modrm = mem[pc+1]; unsigned short ip_off = r->ip+2;
            unsigned short* reg = reg16_by_index(r,(modrm>>3)&7);
            unsigned short tmp = *reg, val = read_rm16(r,modrm,mem,ip_off);
            *reg = val;
            unsigned short ip_tmp = r->ip+2; write_rm16(r,modrm,mem,ip_tmp,tmp);
            r->ip = ip_off; return 0;
        }
        case 0x00: { // ADD r/m8, r8
            unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2;
            unsigned char src=get_reg8(r,(modrm>>3)&7);
            if (((modrm>>6)&3)==3) { unsigned short res=(unsigned short)get_reg8(r,modrm&7)+src; set_reg8(r,modrm&7,(unsigned char)res); set_zf(r,(res&0xFF)==0); set_cf(r,res>0xFF); }
            else { unsigned int addr=get_effective_address(r,modrm,mem,ip_off); unsigned short res=(unsigned short)mem[addr]+src; write_mem8(mem,addr,(unsigned char)res); set_zf(r,(res&0xFF)==0); set_cf(r,res>0xFF); }
            r->ip=ip_off; return 0;
        }
        case 0x01: { // ADD r/m16, r16
            unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2;
            unsigned short src=*reg16_by_index(r,(modrm>>3)&7), val=read_rm16(r,modrm,mem,ip_off);
            unsigned int res=(unsigned int)val+src;
            unsigned short ip_tmp=r->ip+2; write_rm16(r,modrm,mem,ip_tmp,(unsigned short)res);
            set_zf(r,(res&0xFFFF)==0); set_cf(r,res>0xFFFF); r->ip=ip_off; return 0;
        }
        case 0x02: { // ADD r8, r/m8
            unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2;
            unsigned char src=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)];
            unsigned short res=(unsigned short)get_reg8(r,(modrm>>3)&7)+src;
            set_reg8(r,(modrm>>3)&7,(unsigned char)res); set_zf(r,(res&0xFF)==0); set_cf(r,res>0xFF);
            r->ip=ip_off; return 0;
        }
        case 0x03: { // ADD r16, r/m16
            unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2;
            unsigned short src=read_rm16(r,modrm,mem,ip_off); unsigned short* dst=reg16_by_index(r,(modrm>>3)&7);
            unsigned int res=(unsigned int)*dst+src; *dst=(unsigned short)res;
            set_zf(r,*dst==0); set_cf(r,res>0xFFFF); r->ip=ip_off; return 0;
        }
        case 0x04: { unsigned short res=(unsigned short)(r->ax&0xFF)+mem[pc+1]; set_reg8(r,0,(unsigned char)res); set_zf(r,(res&0xFF)==0); set_cf(r,res>0xFF); r->ip+=2; return 0; }
        case 0x05: { unsigned int res=(unsigned int)r->ax+read_mem16(mem,linear(r->cs,r->ip+1)); r->ax=(unsigned short)res; set_zf(r,r->ax==0); set_cf(r,res>0xFFFF); r->ip+=3; return 0; }
        case 0x08: { unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2; unsigned char src=get_reg8(r,(modrm>>3)&7); if(((modrm>>6)&3)==3){unsigned char res=get_reg8(r,modrm&7)|src;set_reg8(r,modrm&7,res);set_zf(r,res==0);}else{unsigned int addr=get_effective_address(r,modrm,mem,ip_off);unsigned char res=mem[addr]|src;write_mem8(mem,addr,res);set_zf(r,res==0);} r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x09: { unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2; unsigned short src=*reg16_by_index(r,(modrm>>3)&7),val=read_rm16(r,modrm,mem,ip_off),res=val|src; unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x0A: { unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2; unsigned char src=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)]; unsigned char res=get_reg8(r,(modrm>>3)&7)|src;set_reg8(r,(modrm>>3)&7,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x0B: { unsigned char modrm=mem[pc+1]; unsigned short ip_off=r->ip+2; unsigned short*dst=reg16_by_index(r,(modrm>>3)&7); *dst|=read_rm16(r,modrm,mem,ip_off);set_zf(r,*dst==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x0C: { unsigned char res=(unsigned char)(r->ax&0xFF)|mem[pc+1];set_reg8(r,0,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip+=2;return 0; }
        case 0x0D: { r->ax|=read_mem16(mem,linear(r->cs,r->ip+1));set_zf(r,r->ax==0);r->flags&=~0x0001;r->ip+=3;return 0; }
        case 0x10: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char cf=(r->flags&1)?1:0,src=get_reg8(r,(modrm>>3)&7); if(((modrm>>6)&3)==3){unsigned short res=(unsigned short)get_reg8(r,modrm&7)+src+cf;set_reg8(r,modrm&7,(unsigned char)res);set_zf(r,(res&0xFF)==0);set_cf(r,res>0xFF);}else{unsigned int addr=get_effective_address(r,modrm,mem,ip_off);unsigned short res=(unsigned short)mem[addr]+src+cf;write_mem8(mem,addr,(unsigned char)res);set_zf(r,(res&0xFF)==0);set_cf(r,res>0xFF);}r->ip=ip_off;return 0; }
        case 0x11: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2,cf=(r->flags&1)?1:0;unsigned short src=*reg16_by_index(r,(modrm>>3)&7),val=read_rm16(r,modrm,mem,ip_off);unsigned int res=(unsigned int)val+src+cf;unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,(unsigned short)res);set_zf(r,(res&0xFFFF)==0);set_cf(r,res>0xFFFF);r->ip=ip_off;return 0; }
        case 0x12: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char cf=(r->flags&1)?1:0,src=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)];unsigned short res=(unsigned short)get_reg8(r,(modrm>>3)&7)+src+cf;set_reg8(r,(modrm>>3)&7,(unsigned char)res);set_zf(r,(res&0xFF)==0);set_cf(r,res>0xFF);r->ip=ip_off;return 0; }
        case 0x13: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2,cf=(r->flags&1)?1:0;unsigned short*dst=reg16_by_index(r,(modrm>>3)&7);unsigned int res=(unsigned int)*dst+read_rm16(r,modrm,mem,ip_off)+cf;*dst=(unsigned short)res;set_zf(r,*dst==0);set_cf(r,res>0xFFFF);r->ip=ip_off;return 0; }
        case 0x14: { unsigned short res=(unsigned short)(r->ax&0xFF)+mem[pc+1]+((r->flags&1)?1:0);set_reg8(r,0,(unsigned char)res);set_zf(r,(res&0xFF)==0);set_cf(r,res>0xFF);r->ip+=2;return 0; }
        case 0x15: { unsigned int res=(unsigned int)r->ax+read_mem16(mem,linear(r->cs,r->ip+1))+((r->flags&1)?1:0);r->ax=(unsigned short)res;set_zf(r,r->ax==0);set_cf(r,res>0xFFFF);r->ip+=3;return 0; }
        case 0x28: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=get_reg8(r,(modrm>>3)&7); if(((modrm>>6)&3)==3){unsigned char val=get_reg8(r,modrm&7),res=val-src;set_reg8(r,modrm&7,res);set_zf(r,res==0);set_cf(r,val<src);}else{unsigned int addr=get_effective_address(r,modrm,mem,ip_off);unsigned char val=mem[addr],res=val-src;write_mem8(mem,addr,res);set_zf(r,res==0);set_cf(r,val<src);}r->ip=ip_off;return 0; }
        case 0x29: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short src=*reg16_by_index(r,(modrm>>3)&7),val=read_rm16(r,modrm,mem,ip_off),res=val-src;unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,res);set_zf(r,res==0);set_cf(r,val<src);r->ip=ip_off;return 0; }
        case 0x2A: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)];unsigned char dst=get_reg8(r,(modrm>>3)&7),res=dst-src;set_reg8(r,(modrm>>3)&7,res);set_zf(r,res==0);set_cf(r,dst<src);r->ip=ip_off;return 0; }
        case 0x2B: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short src=read_rm16(r,modrm,mem,ip_off);unsigned short*dst=reg16_by_index(r,(modrm>>3)&7);set_cf(r,*dst<src);*dst-=src;set_zf(r,*dst==0);r->ip=ip_off;return 0; }
        case 0x2C: { unsigned char imm=mem[pc+1],al=(unsigned char)(r->ax&0xFF);set_cf(r,al<imm);al-=imm;set_reg8(r,0,al);set_zf(r,al==0);r->ip+=2;return 0; }
        case 0x2D: { unsigned short imm=read_mem16(mem,linear(r->cs,r->ip+1));set_cf(r,r->ax<imm);r->ax-=imm;set_zf(r,r->ax==0);r->ip+=3;return 0; }
        case 0x20: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=get_reg8(r,(modrm>>3)&7); if(((modrm>>6)&3)==3){unsigned char res=get_reg8(r,modrm&7)&src;set_reg8(r,modrm&7,res);set_zf(r,res==0);}else{unsigned int addr=get_effective_address(r,modrm,mem,ip_off);unsigned char res=mem[addr]&src;write_mem8(mem,addr,res);set_zf(r,res==0);}r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x21: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short src=*reg16_by_index(r,(modrm>>3)&7),val=read_rm16(r,modrm,mem,ip_off),res=val&src;unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x22: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)],res=get_reg8(r,(modrm>>3)&7)&src;set_reg8(r,(modrm>>3)&7,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x23: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short*dst=reg16_by_index(r,(modrm>>3)&7);*dst&=read_rm16(r,modrm,mem,ip_off);set_zf(r,*dst==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x24: { unsigned char res=(unsigned char)(r->ax&0xFF)&mem[pc+1];set_reg8(r,0,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip+=2;return 0; }
        case 0x25: { r->ax&=read_mem16(mem,linear(r->cs,r->ip+1));set_zf(r,r->ax==0);r->flags&=~0x0001;r->ip+=3;return 0; }
        case 0x30: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=get_reg8(r,(modrm>>3)&7); if(((modrm>>6)&3)==3){unsigned char res=get_reg8(r,modrm&7)^src;set_reg8(r,modrm&7,res);set_zf(r,res==0);}else{unsigned int addr=get_effective_address(r,modrm,mem,ip_off);unsigned char res=mem[addr]^src;write_mem8(mem,addr,res);set_zf(r,res==0);}r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x31: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short src=*reg16_by_index(r,(modrm>>3)&7),val=read_rm16(r,modrm,mem,ip_off),res=val^src;unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x32: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)],res=get_reg8(r,(modrm>>3)&7)^src;set_reg8(r,(modrm>>3)&7,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x33: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short*dst=reg16_by_index(r,(modrm>>3)&7);*dst^=read_rm16(r,modrm,mem,ip_off);set_zf(r,*dst==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x34: { unsigned char res=(unsigned char)(r->ax&0xFF)^mem[pc+1];set_reg8(r,0,res);set_zf(r,res==0);r->flags&=~0x0001;r->ip+=2;return 0; }
        case 0x35: { r->ax^=read_mem16(mem,linear(r->cs,r->ip+1));set_zf(r,r->ax==0);r->flags&=~0x0001;r->ip+=3;return 0; }
        case 0x38: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=get_reg8(r,(modrm>>3)&7),val=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)];set_zf(r,val==src);set_cf(r,val<src);r->ip=ip_off;return 0; }
        case 0x39: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short src=*reg16_by_index(r,(modrm>>3)&7),val=read_rm16(r,modrm,mem,ip_off);set_zf(r,val==src);set_cf(r,val<src);r->ip=ip_off;return 0; }
        case 0x3A: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)],val=get_reg8(r,(modrm>>3)&7);set_zf(r,val==src);set_cf(r,val<src);r->ip=ip_off;return 0; }
        case 0x3B: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short src=read_rm16(r,modrm,mem,ip_off),val=*reg16_by_index(r,(modrm>>3)&7);set_zf(r,val==src);set_cf(r,val<src);r->ip=ip_off;return 0; }
        case 0x3C: { unsigned char imm=mem[pc+1],al=(unsigned char)(r->ax&0xFF);set_zf(r,al==imm);set_cf(r,al<imm);r->ip+=2;return 0; }
        case 0x3D: { unsigned short imm=read_mem16(mem,linear(r->cs,r->ip+1));set_zf(r,r->ax==imm);set_cf(r,r->ax<imm);r->ip+=3;return 0; }
        
        // ============================================================
        //                              TEST
        // ============================================================
        case 0x84: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned char src=get_reg8(r,(modrm>>3)&7),val=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)];set_zf(r,(val&src)==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0x85: { unsigned char modrm=mem[pc+1];unsigned short ip_off=r->ip+2;unsigned short src=*reg16_by_index(r,(modrm>>3)&7),val=read_rm16(r,modrm,mem,ip_off);set_zf(r,(val&src)==0);r->flags&=~0x0001;r->ip=ip_off;return 0; }
        case 0xA8: { set_zf(r,((r->ax&0xFF)&mem[pc+1])==0);r->flags&=~0x0001;r->ip+=2;return 0; }
        case 0xA9: { set_zf(r,(r->ax&read_mem16(mem,linear(r->cs,r->ip+1)))==0);r->flags&=~0x0001;r->ip+=3;return 0; }
        case 0x80: { // ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m8, imm8
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_off=r->ip+2;
            unsigned char imm=mem[linear(r->cs,ip_off)];
            unsigned char val=(((modrm>>6)&3)==3)?get_reg8(r,modrm&7):mem[get_effective_address(r,modrm,mem,ip_off)];
            unsigned char res=val;
            switch(subop){
                case 0:res=val+imm;set_cf(r,(unsigned short)val+imm>0xFF);break;
                case 1:res=val|imm;set_cf(r,false);break;
                case 2:res=val+imm+((r->flags&1)?1:0);set_cf(r,(unsigned short)val+imm>0xFF);break;
                case 3:res=val-imm-((r->flags&1)?1:0);set_cf(r,val<imm);break;
                case 4:res=val&imm;set_cf(r,false);break;
                case 5:res=val-imm;set_cf(r,val<imm);break;
                case 6:res=val^imm;set_cf(r,false);break;
                case 7:set_zf(r,val==imm);set_cf(r,val<imm);r->ip=ip_off+1;return 0; // CMP
            }
            if(((modrm>>6)&3)==3) set_reg8(r,modrm&7,res);
            else { unsigned short ip_tmp=r->ip+2; write_mem8(mem,get_effective_address(r,modrm,mem,ip_tmp),res); }
            set_zf(r,res==0); r->ip=ip_off+1; return 0;
        }
        case 0x81: { // ADD/OR/.../CMP r/m16, imm16
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_off=r->ip+2;
            unsigned short imm=read_mem16(mem,linear(r->cs,ip_off)),val=read_rm16(r,modrm,mem,ip_off);
            unsigned short res=val;
            switch(subop){
                case 0:{unsigned int r2=(unsigned int)val+imm;res=(unsigned short)r2;set_cf(r,r2>0xFFFF);}break;
                case 1:res=val|imm;set_cf(r,false);break;
                case 2:{unsigned int r2=(unsigned int)val+imm+((r->flags&1)?1:0);res=(unsigned short)r2;set_cf(r,r2>0xFFFF);}break;
                case 3:res=val-imm-((r->flags&1)?1:0);set_cf(r,val<imm);break;
                case 4:res=val&imm;set_cf(r,false);break;
                case 5:res=val-imm;set_cf(r,val<imm);break;
                case 6:res=val^imm;set_cf(r,false);break;
                case 7:set_zf(r,val==imm);set_cf(r,val<imm);r->ip=ip_off+2;return 0;
            }
            unsigned short ip_tmp=r->ip+2; write_rm16(r,modrm,mem,ip_tmp,res);
            set_zf(r,res==0); r->ip=ip_off+2; return 0;
        }
        case 0x83: { // ADD/OR/.../CMP r/m16, imm8 signé
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_off=r->ip+2;
            signed char simm=(signed char)mem[linear(r->cs,ip_off)];
            unsigned short imm=(unsigned short)(signed short)simm;
            unsigned short val=read_rm16(r,modrm,mem,ip_off),res=val;
            switch(subop){
                case 0:res=val+imm;set_cf(r,(unsigned int)val+imm>0xFFFF);break;
                case 1:res=val|imm;set_cf(r,false);break;
                case 2:res=val+imm+((r->flags&1)?1:0);break;
                case 3:res=val-imm-((r->flags&1)?1:0);set_cf(r,val<imm);break;
                case 4:res=val&imm;set_cf(r,false);break;
                case 5:res=val-imm;set_cf(r,val<imm);break;
                case 6:res=val^imm;set_cf(r,false);break;
                case 7:set_zf(r,val==imm);set_cf(r,val<imm);r->ip=ip_off+1;return 0;
            }
            unsigned short ip_tmp=r->ip+2; write_rm16(r,modrm,mem,ip_tmp,res);
            set_zf(r,res==0); r->ip=ip_off+1; return 0;
        }
        case 0xC0: { // Shifts r/m8, imm8
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7,cl=mem[pc+2];
            unsigned char v=get_reg8(r,modrm&7);
            switch(subop){case 4:v<<=cl;break;case 5:v>>=cl;break;case 7:v=(unsigned char)((signed char)v>>cl);break;}
            set_reg8(r,modrm&7,v);set_zf(r,v==0);r->ip+=3;return 0;
        }
        case 0xC1: { // Shifts r/m16, imm8
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7,cl=mem[pc+2];
            unsigned short ip_off=r->ip+2,v=read_rm16(r,modrm,mem,ip_off);
            switch(subop){case 4:v<<=cl;break;case 5:v>>=cl;break;case 7:v=(unsigned short)((signed short)v>>cl);break;}
            ip_off=r->ip+2;write_rm16(r,modrm,mem,ip_off,v);set_zf(r,v==0);r->ip+=3;return 0;
        }
        case 0xD0: { // Shifts r/m8, 1
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_tmp = r->ip + 2;
            unsigned char v = (((modrm>>6)&3)==3) ? get_reg8(r,modrm&7): mem[get_effective_address(r,modrm,mem,ip_tmp)];
            switch(subop){case 4:set_cf(r,(v&0x80)!=0);v<<=1;break;case 5:set_cf(r,(v&1)!=0);v>>=1;break;case 7:v=(unsigned char)((signed char)v>>1);break;}
            if(((modrm>>6)&3)==3)set_reg8(r,modrm&7,v);else{unsigned short ip_tmp=r->ip+2;write_mem8(mem,get_effective_address(r,modrm,mem,ip_tmp),v);}
            set_zf(r,v==0);r->ip+=2;return 0;
        }
        case 0xD1: { // Shifts r/m16, 1
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_off=r->ip+2,v=read_rm16(r,modrm,mem,ip_off);
            switch(subop){case 4:set_cf(r,(v&0x8000)!=0);v<<=1;break;case 5:set_cf(r,(v&1)!=0);v>>=1;break;case 7:v=(unsigned short)((signed short)v>>1);break;}
            ip_off=r->ip+2;write_rm16(r,modrm,mem,ip_off,v);set_zf(r,v==0);r->ip+=2;return 0;
        }
        case 0xD2: { // Shifts r/m8, CL
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7,cl=(unsigned char)(r->cx&0xFF);
            unsigned char v=get_reg8(r,modrm&7);
            switch(subop){case 4:v<<=cl;break;case 5:v>>=cl;break;case 7:v=(unsigned char)((signed char)v>>cl);break;}
            set_reg8(r,modrm&7,v);set_zf(r,v==0);r->ip+=2;return 0;
        }
        case 0xD3: { // Shifts r/m16, CL
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7,cl=(unsigned char)(r->cx&0xFF);
            unsigned short ip_off=r->ip+2,v=read_rm16(r,modrm,mem,ip_off);
            switch(subop){case 4:v<<=cl;break;case 5:v>>=cl;break;case 7:v=(unsigned short)((signed short)v>>cl);break;}
            ip_off=r->ip+2;write_rm16(r,modrm,mem,ip_off,v);set_zf(r,v==0);r->ip+=2;return 0;
        }
        case 0xF6: { 
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_tmp = r->ip + 2;
            unsigned char v = (((modrm>>6)&3)==3) ? get_reg8(r,modrm&7)
                : mem[get_effective_address(r,modrm,mem,ip_tmp)];
            switch(subop)
            {
                case 0:
                case 1:{
                    unsigned char imm=mem[pc+2];
                    set_zf(r,(v&imm)==0);
                    r->flags&=~0x0001;
                    r->ip+=3;
                    return 0;
                }
                case 2:{
                    v=~v;
                    if(((modrm>>6)&3)==3)
                        set_reg8(r,modrm&7,v);
                        else{
                            unsigned short ip_tmp=r->ip+2;
                            write_mem8(mem,get_effective_address(r,modrm,mem,ip_tmp),v);
                        }r->ip+=2;
                        return 0;
                    }
                    case 3:{v=(unsigned char)(-(signed char)v);
                        if(((modrm>>6)&3)==3)set_reg8(r,modrm&7,v);
                        else
                        {unsigned short ip_tmp=r->ip+2;
                            write_mem8(mem,get_effective_address(r,modrm,mem,ip_tmp),v);
                        }set_zf(r,v==0);
                        r->ip+=2;
                        return 0;
                    }
                    case 4:{
                        unsigned short res=(unsigned short)((r->ax&0xFF)*v);
                        r->ax=res;r->ip+=2;
                        return 0;
                    }
                    case 5:
                    {
                        signed short res=(signed short)((signed char)(r->ax&0xFF)*(signed char)v);
                        r->ax=(unsigned short)res;
                        r->ip+=2;
                        return 0;
                    }
                    case 6:
                    if(v==0)
                    {
                        trace_fault(r,op,modrm,-10);return -10;
                    }
                    r->ax=(unsigned short)(((unsigned char)(r->ax&0xFF)/v)|((unsigned char)(r->ax&0xFF)%v)<<8);
                    r->ip+=2;
                    return 0;
                }
            r->ip+=2;return 0;
        }
        case 0xF7: { // MUL/DIV/NOT/NEG r/m16
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_off=r->ip+2,val=read_rm16(r,modrm,mem,ip_off);
            switch(subop){
                case 0:case 1:{unsigned short imm=read_mem16(mem,linear(r->cs,ip_off));set_zf(r,(val&imm)==0);r->flags&=~0x0001;r->ip=ip_off+2;return 0;}
                case 2:{unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,~val);r->ip=ip_off;return 0;}
                case 3:{unsigned short res=(unsigned short)(-(signed short)val);unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,res);set_zf(r,res==0);r->ip=ip_off;return 0;}
                case 4:{unsigned int res=(unsigned int)r->ax*val;r->ax=(unsigned short)(res&0xFFFF);r->dx=(unsigned short)(res>>16);r->ip=ip_off;return 0;}
                case 5:{int res=(signed short)r->ax*(signed short)val;r->ax=(unsigned short)(res&0xFFFF);r->dx=(unsigned short)((unsigned int)res>>16);r->ip=ip_off;return 0;}
                case 6:if(!val){trace_fault(r,op,modrm,-10);return -10;}{unsigned int num=((unsigned int)r->dx<<16)|r->ax;r->ax=(unsigned short)(num/val);r->dx=(unsigned short)(num%val);r->ip=ip_off;return 0;}
                case 7:if(!val){trace_fault(r,op,modrm,-10);return -10;}{int num=(int)(((unsigned int)r->dx<<16)|r->ax);r->ax=(unsigned short)(num/(signed short)val);r->dx=(unsigned short)(num%(signed short)val);r->ip=ip_off;return 0;}
            }
            r->ip=ip_off;return 0;
        }
        case 0xFE: { unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;unsigned short ip_off=r->ip+2; unsigned short ip_tmp = r->ip + 2;
unsigned char v = (((modrm>>6)&3)==3) ? get_reg8(r,modrm&7)
    : mem[get_effective_address(r,modrm,mem,ip_tmp)];if(subop==0)v++;else if(subop==1)v--;else{trace_fault(r,op,modrm,-10);return -10;}if(((modrm>>6)&3)==3)set_reg8(r,modrm&7,v);else{unsigned short ip_tmp=r->ip+2;write_mem8(mem,get_effective_address(r,modrm,mem,ip_tmp),v);}set_zf(r,v==0);r->ip=ip_off;return 0; }
        
        case 0xFF: {
            unsigned char modrm=mem[pc+1],subop=(modrm>>3)&7;
            unsigned short ip_off=r->ip+2,val=read_rm16(r,modrm,mem,ip_off);
            switch(subop){
                case 0:{unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,val+1);set_zf(r,val+1==0);r->ip=ip_off;return 0;}
                case 1:{unsigned short ip_tmp=r->ip+2;write_rm16(r,modrm,mem,ip_tmp,val-1);set_zf(r,val-1==0);r->ip=ip_off;return 0;}
                case 2:{r->sp-=2;write_mem16(mem,linear(r->ss,r->sp),ip_off);r->ip=val;return 0;}
                case 3:{unsigned short ip_tmp = r->ip + 2;
unsigned char addr = (((modrm>>6)&3)==3) ? get_reg8(r,modrm&7)
    : mem[get_effective_address(r,modrm,mem,ip_tmp)];
    unsigned short nip=read_mem16(mem,addr),ncs=read_mem16(mem,addr+2);r->sp-=2;write_mem16(mem,linear(r->ss,r->sp),r->cs);r->sp-=2;write_mem16(mem,linear(r->ss,r->sp),ip_off);r->ip=nip;r->cs=ncs;return 0;}
                case 4:{r->ip=val;return 0;}
                case 5:{unsigned short ip_tmp = r->ip + 2;
unsigned char addr = (((modrm>>6)&3)==3) ? get_reg8(r,modrm&7)
    : mem[get_effective_address(r,modrm,mem,ip_tmp)];r->ip=read_mem16(mem,addr);r->cs=read_mem16(mem,addr+2);return 0;}
                case 6:{r->sp-=2;write_mem16(mem,linear(r->ss,r->sp),val);r->ip=ip_off;return 0;}
                default:trace_fault(r,0xFF,modrm,-10);return -10;
            }
        }
        case 0xE8: { signed short rel=(signed short)read_mem16(mem,linear(r->cs,r->ip+1)); r->sp-=2;write_mem16(mem,linear(r->ss,r->sp),(unsigned short)(r->ip+3)); r->ip=(unsigned short)(r->ip+3+rel);return 0; }
        case 0xE9: { signed short rel=(signed short)read_mem16(mem,linear(r->cs,r->ip+1)); r->ip=(unsigned short)(r->ip+3+rel);return 0; }
        case 0xEA: { unsigned short nip=read_mem16(mem,linear(r->cs,r->ip+1)),ncs=read_mem16(mem,linear(r->cs,r->ip+3)); r->ip=nip;r->cs=ncs;return 0; }
        case 0xEB: { r->ip=(unsigned short)(r->ip+2+(signed char)mem[pc+1]);return 0; }
        case 0x9A: { unsigned short nip=read_mem16(mem,linear(r->cs,r->ip+1)),ncs=read_mem16(mem,linear(r->cs,r->ip+3)); r->sp-=2;write_mem16(mem,linear(r->ss,r->sp),r->cs);r->sp-=2;write_mem16(mem,linear(r->ss,r->sp),(unsigned short)(r->ip+5));r->ip=nip;r->cs=ncs;return 0; }
        case 0xC2: { unsigned short imm=read_mem16(mem,linear(r->cs,r->ip+1)); r->ip=read_mem16(mem,linear(r->ss,r->sp));r->sp+=2+imm;return 0; }
        case 0xC3: { r->ip=read_mem16(mem,linear(r->ss,r->sp));r->sp+=2;return 0; }
        case 0xCA: { unsigned short imm=read_mem16(mem,linear(r->cs,r->ip+1)); r->ip=read_mem16(mem,linear(r->ss,r->sp));r->sp+=2;r->cs=read_mem16(mem,linear(r->ss,r->sp));r->sp+=2+imm;return 0; }
        case 0xCB: { r->ip=read_mem16(mem,linear(r->ss,r->sp));r->sp+=2;r->cs=read_mem16(mem,linear(r->ss,r->sp));r->sp+=2;return 0; }
        case 0x70: { signed char rel=mem[pc+1]; if(r->flags&0x800) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x71: { signed char rel=mem[pc+1]; if(!(r->flags&0x800)) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x72: { signed char rel=mem[pc+1]; if(r->flags&0x1) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x73: { signed char rel=mem[pc+1]; if(!(r->flags&0x1)) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x74: { signed char rel=mem[pc+1]; if(r->flags&0x40) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x75: { signed char rel=mem[pc+1]; if(!(r->flags&0x40)) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x76: { signed char rel=mem[pc+1]; if((r->flags&0x1)||(r->flags&0x40)) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x77: { signed char rel=mem[pc+1]; if(!(r->flags&0x1)&&!(r->flags&0x40)) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x78: { signed char rel=mem[pc+1]; if(r->flags&0x80) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x79: { signed char rel=mem[pc+1]; if(!(r->flags&0x80)) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x7A: { signed char rel=mem[pc+1]; if(r->flags&0x4) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x7B: { signed char rel=mem[pc+1]; if(!(r->flags&0x4)) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x7C: { signed char rel=mem[pc+1]; bool sf=(r->flags&0x80)!=0,of=(r->flags&0x800)!=0; if(sf!=of) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x7D: { signed char rel=mem[pc+1]; bool sf=(r->flags&0x80)!=0,of=(r->flags&0x800)!=0; if(sf==of) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x7E: { signed char rel=mem[pc+1]; bool zf=(r->flags&0x40)!=0,sf=(r->flags&0x80)!=0,of=(r->flags&0x800)!=0; if(zf||sf!=of) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0x7F: { signed char rel=mem[pc+1]; bool zf=(r->flags&0x40)!=0,sf=(r->flags&0x80)!=0,of=(r->flags&0x800)!=0; if(!zf&&sf==of) r->ip=(unsigned short)(r->ip+2+rel); else r->ip+=2; return 0; }
        case 0xE0: { signed char rel=mem[pc+1];r->cx--;if(r->cx!=0&&!(r->flags&0x40))r->ip=(unsigned short)(r->ip+2+rel);else r->ip+=2;return 0; }
        case 0xE1: { signed char rel=mem[pc+1];r->cx--;if(r->cx!=0&&(r->flags&0x40))r->ip=(unsigned short)(r->ip+2+rel);else r->ip+=2;return 0; }
        case 0xE2: { signed char rel=mem[pc+1];r->cx--;if(r->cx!=0)r->ip=(unsigned short)(r->ip+2+rel);else r->ip+=2;return 0; }
        case 0xE3: { signed char rel=mem[pc+1];if(r->cx==0)r->ip=(unsigned short)(r->ip+2+rel);else r->ip+=2;return 0; }
        case 0x27: { unsigned char al=(unsigned char)(r->ax&0xFF);if((al&0xF)>9||(r->flags&0x10))al+=6;if(al>0x9F||(r->flags&0x1)){al+=0x60;set_cf(r,true);}set_reg8(r,0,al);set_zf(r,al==0);r->ip+=1;return 0; }
        case 0x2F: { unsigned char al=(unsigned char)(r->ax&0xFF);if((al&0xF)>9||(r->flags&0x10))al-=6;if(al>0x9F||(r->flags&0x1)){al-=0x60;set_cf(r,true);}set_reg8(r,0,al);set_zf(r,al==0);r->ip+=1;return 0; }
        case 0x37: { if((r->ax&0xF)>9||(r->flags&0x10)){r->ax+=0x106;r->flags|=0x11;}else r->flags&=~0x11;r->ax&=0xFF0F;r->ip+=1;return 0; }
        case 0x98: { r->ax=(unsigned short)(signed short)(signed char)(r->ax&0xFF);r->ip+=1;return 0; }
        case 0x99: { r->dx=(r->ax&0x8000)?0xFFFF:0;r->ip+=1;return 0; }
        case 0xD4: { unsigned char base=mem[pc+1];if(!base){trace_fault(r,op,0,-10);return -10;}unsigned char al=(unsigned char)(r->ax&0xFF);r->ax=(unsigned short)(((al/base)<<8)|(al%base));set_zf(r,(r->ax&0xFF)==0);r->ip+=2;return 0; }
        case 0xD5: { unsigned char base=mem[pc+1],al=(unsigned char)(r->ax&0xFF),ah=(unsigned char)((r->ax>>8)&0xFF);al=(unsigned char)(ah*base+al);r->ax=(unsigned short)al;set_zf(r,al==0);r->ip+=2;return 0; }
        case 0xD7: { set_reg8(r,0,mem[linear(r->ds,(unsigned short)(r->bx+(unsigned char)(r->ax&0xFF)))]);r->ip+=1;return 0; }
        

        case 0xA4: { // MOVSB
            short inc=(r->flags&0x400)?-1:1;
            mem[linear(r->es,r->di)]=mem[linear(r->ds,r->si)];
            r->si+=inc;r->di+=inc;r->ip+=1;return 0;
        }
        case 0xA5: { // MOVSW
            short inc=(r->flags&0x400)?-2:2;
            write_mem16(mem,linear(r->es,r->di),read_mem16(mem,linear(r->ds,r->si)));
            r->si+=inc;r->di+=inc;r->ip+=1;return 0;
        }
        case 0xAA: { // STOSB
            short inc=(r->flags&0x400)?-1:1;
            write_mem8(mem,linear(r->es,r->di),(unsigned char)(r->ax&0xFF));
            r->di+=inc;r->ip+=1;return 0;
        }
        case 0xAB: { // STOSW
            short inc=(r->flags&0x400)?-2:2;
            write_mem16(mem,linear(r->es,r->di),r->ax);
            r->di+=inc;r->ip+=1;return 0;
        }
        case 0xAC: { // LODSB
            short inc=(r->flags&0x400)?-1:1;
            set_reg8(r,0,mem[linear(r->ds,r->si)]);
            r->si+=inc;r->ip+=1;return 0;
        }
        case 0xAD: { // LODSW
            short inc=(r->flags&0x400)?-2:2;
            r->ax=read_mem16(mem,linear(r->ds,r->si));
            r->si+=inc;r->ip+=1;return 0;
        }
        case 0xAE: { // SCASB — CMP AL, ES:DI
            short inc=(r->flags&0x400)?-1:1;
            unsigned char val=mem[linear(r->es,r->di)],al=(unsigned char)(r->ax&0xFF);
            set_zf(r,al==val);set_cf(r,al<val);
            r->di+=inc;r->ip+=1;return 0;
        }
        
        // ============================================================
        // I/O (IN/OUT)
        // ============================================================
        case 0xE4: { // IN AL, imm8
            unsigned char port=mem[pc+1];
            if(port_in_cb) set_reg8(r,0,port_in_cb(port,port_ctx));
            r->ip+=2;return 0;
        }
        case 0xE5: { // IN AX, imm8
            unsigned char port=mem[pc+1];
            if(port_in_cb) r->ax=port_in_cb(port,port_ctx);
            r->ip+=2;return 0;
        }
        case 0xE6: { // OUT imm8, AL
            unsigned char port=mem[pc+1];
            if(port_out_cb) port_out_cb(port,(unsigned char)(r->ax&0xFF),port_ctx);
            r->ip+=2;return 0;
        }
        case 0xE7: { // OUT imm8, AX
            unsigned char port=mem[pc+1];
            if(port_out_cb){port_out_cb(port,(unsigned char)(r->ax&0xFF),port_ctx);port_out_cb(port+1,(unsigned char)(r->ax>>8),port_ctx);}
            r->ip+=2;return 0;
        }
        case 0xEC: { // IN AL, DX
            if(port_in_cb) set_reg8(r,0,port_in_cb((unsigned char)(r->dx&0xFF),port_ctx));
            r->ip+=1;return 0;
        }
        case 0xEE: { // OUT DX, AL
            if(port_out_cb) port_out_cb(r->dx,(unsigned char)(r->ax&0xFF),port_ctx);
            r->ip+=1;return 0;
        }
        case 0xEF: { // OUT DX, AX
            if(port_out_cb){port_out_cb(r->dx,(unsigned char)(r->ax&0xFF),port_ctx);port_out_cb(r->dx+1,(unsigned char)(r->ax>>8),port_ctx);}
            r->ip+=1;return 0;
        }
        
        // ============================================================
        // INT (0xCD) — syscalls DOS/BIOS
        // ============================================================
        case 0xCD: {
            unsigned char intn=mem[pc+1]; r->ip+=2;
            if(intn==0x20){halt=true;exit_code=0;return 0;}
            if(intn==0x21) return handle_int21(mem,r,halt,exit_code);
            if((intn==0x10||intn==0x16||intn==0x1A)&&host_bios_int){
                int brc=host_bios_int(intn,r,host_bios_ctx);
                if(brc==0) return 0;
            }
            set_cf(r,false); return 0;
        }
        
        // ============================================================
        // FPU — tout ignorer (D8-DF)
        // ============================================================
        case 0xD8: case 0xD9: case 0xDA: case 0xDB:
        case 0xDC: case 0xDD: case 0xDE: case 0xDF: {
            unsigned char modrm=mem[pc+1];
            r->ip+=((modrm>>6)&3)==3 ? 2 : 4;
            return 0;
        }
        
        // ============================================================
        // Default — opcode non implémenté
        // ============================================================
        default:
            trace_fault(r, op, 0, -10);
            return -10;
        
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

        // Détecteur de boucle — si on revient au même CS:IP 1000 fois
        unsigned int last_cs = 0xFFFF, last_ip = 0xFFFF;
        unsigned int repeat_count = 0;

        for (unsigned int i = 0; i < max_steps && !halt; i++) {
            // Détecter boucle infinie
            if (regs->cs == last_cs && regs->ip == last_ip) {
                repeat_count++;
                if (repeat_count > 10000) {
                    // Afficher où on boucle
                    last_trace.cs  = regs->cs;
                    last_trace.ip  = regs->ip;
                    unsigned int pc = linear(regs->cs, regs->ip);
                    last_trace.opcode = mem[pc];
                    last_trace.reason = -30;
                    return -30;  // Boucle infinie détectée
                }
            } else {
                last_cs = regs->cs;
                last_ip = regs->ip;
                repeat_count = 0;
            }

            int rc = emulate_step(mem, regs, halt, exit_code);
            if (rc != 0) return rc;
        }

        if (!halt) return -8;
        if (out_exit) *out_exit = exit_code;
        return 0;
    }
};
