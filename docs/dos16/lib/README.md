# DOS64STD - mini standard library bridge (16-bit DOS)

Cette librairie fournit un pont minimal entre un programme MS-DOS 16-bit et DOS64 via `INT 21h`.

## API
- `dos64_putc(char)` -> `AH=02h`
- `dos64_puts(const char*)` (chaîne terminée par `$`) -> `AH=09h`
- `dos64_get_version()` -> `AH=30h`
- `dos64_exit(code)` -> `AH=4Ch`

## Compilation (exemple MASM)
```bat
masm DOS64STD.ASM;
lib DOS64STD.LIB +DOS64STD.OBJ;
```

## Utilisation (C 16-bit)
```c
#include "DOS64STD.H"

int main(void) {
    dos64_puts("Hello via DOS64STD$\r\n$");
    dos64_exit(0);
    return 0;
}
```

## Pourquoi c'est utile
Cela évite d'écrire les `INT 21h` à la main dans chaque programme et permet d'imiter une mini-`std` portable côté DOS64.
