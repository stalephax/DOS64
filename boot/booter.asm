MULTIBOOT_MAGIC     equ 0x1BADB002
MULTIBOOT_FLAGS     equ 0x00000003
MULTIBOOT_CHECKSUM  equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

section .multiboot
align 4
    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

; =============================================
; STACK (pile 64 bit)
; =============================================
section .bss
align 16
stack_bottom:
    resb 32768          ; 32 Ko de stack
stack_top:

; =============================================
; PAGE TABLES (obligatoires pour le mode long)
; =============================================
align 4096
pml4_table:     resb 4096   ; Page Map Level 4
pdp_table:      resb 4096   ; Page Directory Pointer
pd_table0:      resb 4096   ; PD pour 0..1 Gio
pd_table1:      resb 4096   ; PD pour 1..2 Gio
pd_table2:      resb 4096   ; PD pour 2..3 Gio
pd_table3:      resb 4096   ; PD pour 3..4 Gio

; =============================================
; GDT 64 BIT
; =============================================
section .data
align 8
gdt64:
    dq 0                            ; Entrée nulle obligatoire
.code:
    dq (1<<43)|(1<<44)|(1<<47)|(1<<53)  ; Segment code 64 bit
.end:
gdt64_pointer:
    dw gdt64.end - gdt64 - 1
    dq gdt64

; =============================================
; CODE DE BOOT (32 bit mode)
; =============================================
section .text
bits 32
global _start
extern kernel_main

_start:
    mov esp, stack_top

    ; ← SAUVEGARDER EBX IMMÉDIATEMENT (adresse multiboot GRUB)
    mov esi, ebx        

    ; Vérifier CPUID...
    mov eax, 0x80000000
    cpuid               
    cmp eax, 0x80000001
    jb .no64bit

    mov eax, 0x80000001
    cpuid               ; ← écrase EBX encore
    test edx, (1<<29)
    jz .no64bit

    
; PML4[0] → PDP
    mov eax, pdp_table
    or  eax, 0b11
    mov [pml4_table], eax

    ; PDP[0..3] → PD0..PD3  (identity map 4 Gio via pages 2 Mio)
    mov eax, pd_table0
    or  eax, 0b11
    mov [pdp_table], eax

    mov eax, pd_table1
    or  eax, 0b11
    mov [pdp_table + 8], eax

    mov eax, pd_table2
    or  eax, 0b11
    mov [pdp_table + 16], eax

    mov eax, pd_table3
    or  eax, 0b11
    mov [pdp_table + 24], eax

    ; PD[0..2047] → pages 2 Mio (identity mapping jusqu'à 4 Gio)
    mov ecx, 0
.map_pd:
    mov eax, 0x200000
    imul eax, ecx           ; eax = 2Mo * ecx  (correct même pour ecx=0)
    or   eax, 0b10000011    ; present + writable + huge page
    cmp  ecx, 512
    jb   .pd0
    cmp  ecx, 1024
    jb   .pd1
    cmp  ecx, 1536
    jb   .pd2
    mov  [pd_table3 + (ecx - 1536)*8], eax
    jmp  .next_pd
.pd2:
    mov  [pd_table2 + (ecx - 1024)*8], eax
    jmp  .next_pd
.pd1:
    mov  [pd_table1 + (ecx - 512)*8], eax
    jmp  .next_pd
.pd0:
    mov  [pd_table0 + ecx*8], eax
.next_pd:
    inc  ecx
    cmp  ecx, 2048
    jne  .map_pd

    ; --- 3. Charger CR3 avec PML4 ---
    mov eax, pml4_table
    mov cr3, eax

    ; --- 4. Activer PAE ---
    mov eax, cr4
    or  eax, (1<<5)
    mov cr4, eax

    ; --- 5. Activer Long Mode via MSR ---
    mov ecx, 0xC0000080
    rdmsr
    or  eax, (1<<8)
    wrmsr

    ; --- 6. Activer la pagination + mode protégé ---
    mov eax, cr0
    or  eax, (1<<31)|(1<<0)
    mov cr0, eax

    ; --- 7. Far jump vers le code 64 bit ---
    lgdt [gdt64_pointer]
    jmp 0x08:.mode64

bits 64
.mode64:
    mov ax, 0
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov edi, esi        ; ← ESI a survécu, on le passe à kernel_main
    call kernel_main
    hlt


.no64bit:
    ; CPU trop vieux, on affiche 'X' en rouge et on stoppe
    mov word [0xB8000], 0x4C58
    hlt
