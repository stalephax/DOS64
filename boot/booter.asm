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
pd_table:       resb 4096   ; Page Directory

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

    ; PDP[0] → PD
    mov eax, pd_table
    or  eax, 0b11
    mov [pdp_table], eax

    ; PD[0..511] → pages 2 Mo (identity mapping)
    mov ecx, 0
.map_pd:
    mov eax, 0x200000
    imul eax, ecx           ; eax = 2Mo * ecx  (correct même pour ecx=0)
    or   eax, 0b10000011    ; present + writable + huge page
    mov  [pd_table + ecx*8], eax
    inc  ecx
    cmp  ecx, 512
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