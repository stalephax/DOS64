; DOS64STD.ASM
; Petite bibliothèque standard DOS 16-bit (MZ) pour DOS64.
; Bridge userland -> kernel via INT 21h.
;
; Exported API (cdecl-like simple):
;   dos64_putc(ch)
;   dos64_puts(ptr_dollar_string)
;   dos64_get_version() -> AX
;   dos64_exit(code)
;
; Build idea:
;   masm DOS64STD.ASM;
;   lib DOS64STD.LIB +DOS64STD.OBJ;

.MODEL small
.CODE
PUBLIC dos64_putc
PUBLIC dos64_puts
PUBLIC dos64_get_version
PUBLIC dos64_exit

dos64_putc PROC
    ; arg on stack: [sp+2] char
    push bp
    mov bp, sp
    mov dl, byte ptr [bp+4]
    mov ah, 02h
    int 21h
    pop bp
    ret
 dos64_putc ENDP

dos64_puts PROC
    ; arg on stack: [sp+2] pointer to '$'-terminated string
    push bp
    mov bp, sp
    mov dx, [bp+4]
    mov ah, 09h
    int 21h
    pop bp
    ret
 dos64_puts ENDP

dos64_get_version PROC
    mov ah, 30h
    int 21h
    ret
 dos64_get_version ENDP

dos64_exit PROC
    ; arg on stack: [sp+2] exit code byte
    push bp
    mov bp, sp
    mov al, byte ptr [bp+4]
    mov ah, 4Ch
    int 21h
    ; no return in real DOS, but keep ret fallback
    pop bp
    ret
 dos64_exit ENDP

END
