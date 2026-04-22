bits 64

; Symboles C++ externes
extern interrupt_handler          ; handler pour les exceptions CPU
extern interrupt_handler_syscall  ; handler pour les syscalls (int 0x80)

; =============================================
; SECTION DATA — Table IDT + pointeur
; =============================================
section .data
align 16

global idt_table
idt_table:
    times 256 dq 0, 0       ; 256 entrées vides (16 octets chacune)

global idt_pointer
idt_pointer:
    dw 256 * 16 - 1         ; limite (taille - 1)
    dq idt_table            ; adresse de la table

; =============================================
; SECTION TEXT — Stubs + utilitaires
; =============================================
section .text

; ---------------------------------------------------
; Charger l'IDT (appelé depuis C++)
; ---------------------------------------------------
global idt_load
idt_load:
    lidt [idt_pointer]
    ret

; ---------------------------------------------------
; Macros pour les exceptions CPU
; ISR_NOERR : exceptions sans code d'erreur CPU
; ISR_ERR   : exceptions avec code d'erreur CPU
;             (le CPU pousse lui-même le code sur la stack)
; ---------------------------------------------------
%macro ISR_NOERR 1
global isr_%1
isr_%1:
    push 0                  ; code d'erreur factice pour uniformiser la stack
    push %1                 ; numéro d'exception
    jmp isr_common
%endmacro

%macro ISR_ERR 1
global isr_%1
isr_%1:
    push %1                 ; numéro d'exception (code d'erreur déjà sur la stack)
    jmp isr_common
%endmacro

; ---------------------------------------------------
; Handler commun pour toutes les exceptions CPU
; Stack à l'entrée :
;   [rsp+0]  = int_num
;   [rsp+8]  = error_code
; ---------------------------------------------------
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11

    mov rdi, [rsp + 11*8]   ; 1er arg = int_num
    mov rsi, [rsp + 10*8]   ; 2ème arg = error_code
    call interrupt_handler

    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16             ; enlever int_num + error_code
    iretq

; ---------------------------------------------------
; Exceptions CPU enregistrées
; ---------------------------------------------------
ISR_NOERR 0                 ; #DE Division par zéro
ISR_NOERR 6                 ; #UD Instruction invalide
ISR_ERR   8                 ; #DF Double Fault
ISR_ERR   13                ; #GP General Protection Fault
ISR_ERR   14                ; #PF Page Fault

; ---------------------------------------------------
; Syscall — int 0x80
; Convention d'appel depuis les programmes :
;   RAX = numéro de syscall
;   RDI = argument 1
;   RSI = argument 2
;   RDX = argument 3
; ---------------------------------------------------
global isr_0x80
isr_0x80:
    ; Sauvegarder les registres qui ne sont pas des arguments
    push r11
    push r10
    push r9
    push r8
    push rcx
    push rbx

    ; Réorganiser les arguments pour interrupt_handler_syscall :
    ; C++ attend : (syscall_num, arg1, arg2, arg3)
    ;              (RDI,        RSI,  RDX,  RCX )
    mov rcx, rdx            ; arg3 → RCX (4ème param C++)
    mov rdx, rsi            ; arg2 → RDX (3ème param C++)
    mov rsi, rdi            ; arg1 → RSI (2ème param C++)
    mov rdi, rax            ; syscall_num → RDI (1er param C++)

    call interrupt_handler_syscall

    ; Restaurer les registres
    pop rbx
    pop rcx
    pop r8
    pop r9
    pop r10
    pop r11
    iretq