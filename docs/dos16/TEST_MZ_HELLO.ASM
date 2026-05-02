; DOS64 compatibility test (16-bit MZ EXE)
; -------------------------------------------------
; Purpose:
; - exercise INT 21h AH=09h (print $-terminated string)
; - exercise INT 21h AH=30h (query DOS version)
; - exit through INT 21h AH=4Ch
;
; Syntax: MASM/TASM (small model)

.MODEL small
.STACK 100h

.DATA
msg_hello db ' If you see this, this software works fine !'
msg_ver   db 13,10,'DOS version reported by INT 21h/30h -> major=', '$'
msg_end   db 13,10,'Done.$'
newline   db 13,10,'$'

.CODE

print_char PROC
    ; input: DL=char
    mov ah, 02h
    int 21h
    ret
print_char ENDP

print_nibble PROC
    ; input: AL nibble (0..15)
    and al, 0Fh
    cmp al, 9
    jbe digit
    add al, 7
 digit:
    add al, '0'
    mov dl, al
    call print_char
    ret
print_nibble ENDP

print_hex8 PROC
    ; input: AL byte
    push ax
    mov ah, al
    shr al, 4
    call print_nibble
    mov al, ah
    call print_nibble
    pop ax
    ret
print_hex8 ENDP

start:
    mov ax, @data
    mov ds, ax

    ; 1) print hello
    mov dx, OFFSET msg_hello
    mov ah, 09h
    int 21h

    ; newline
    mov dx, OFFSET newline
    mov ah, 09h
    int 21h

    ; 2) print version prefix
    mov dx, OFFSET msg_ver
    mov ah, 09h
    int 21h

    ; query DOS version (AL=major, AH=minor)
    mov ah, 30h
    int 21h

    ; print major in hex for simplicity
    ; AL already contains major version
    call print_hex8

    ; newline
    mov dx, OFFSET newline
    mov ah, 09h
    int 21h

    ; end message
    mov dx, OFFSET msg_end
    mov ah, 09h
    int 21h

    ; 3) return code 00
    mov ax, 4C00h
    int 21h

END start
