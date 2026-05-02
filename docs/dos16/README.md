# DOS 16-bit MZ smoke test for DOS64

This folder provides a tiny MS-DOS `.EXE` program intended to validate the current DOS64 MZ runtime path.

## Test binary goals

`TEST_MZ_HELLO.ASM` checks the minimum DOS API path currently implemented in DOS64:
- `INT 21h / AH=09h` (print `$` string)
- `INT 21h / AH=30h` (get DOS version)
- `INT 21h / AH=4Ch` (exit)

## Build options

### MASM/TASM style (recommended for this source)
```bat
masm TEST_MZ_HELLO.ASM;
link TEST_MZ_HELLO.OBJ;
```

### OpenWatcom (if available)
```bat
wasm TEST_MZ_HELLO.ASM
wlink system dos name TEST_MZ_HELLO.exe file TEST_MZ_HELLO.obj
```

## Expected behavior in DOS64

When running `RUN TEST_MZ_HELLO.EXE`, expected visible behavior:
1. print hello line,
2. print DOS major version in hex (currently `07` with current DOS64 stub),
3. clean process termination (`exit code 0`).

If the binary crashes, it indicates a regression in MZ load, relocation, or `INT 21h` syscall compatibility.
