CC      = gcc
CXX     = g++
ASM     = nasm
CXXFLAGS = -ffreestanding -fno-exceptions -fno-rtti -nostdlib -mcmodel=kernel -mno-red-zone -fno-pic -Ikernel -Ikernel/drivers -Ikernel/fs
APPFLAGS = -ffreestanding -fno-exceptions -fno-rtti -nostdlib -fno-pic -m64
LDFLAGS  = -T link.ld -nostdlib -z max-page-size=0x1000

OBJS = boot/booter.o boot/idt.o kernel/kernel64.o kernel/standart.o

all: dos64.iso hello.elf gedit.elf keyboard.sys

boot/idt.o: boot/idt.asm
	$(ASM) -f elf64 $< -o $@

boot/booter.o: boot/booter.asm
	$(ASM) -f elf64 $< -o $@

kernel/kernel64.o: kernel/kernel64.cpp kernel/standart.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

dos64.bin: $(OBJS)
	ld -m elf_x86_64 $(LDFLAGS) -o $@ $(OBJS)

dos64.iso: dos64.bin
	mkdir -p iso/boot/grub
	cp dos64.bin iso/boot/
	printf 'set timeout=0\nset default=0\nmenuentry "DOS64" {\n    multiboot /boot/dos64.bin\n    boot\n}\n' > iso/boot/grub/grub.cfg
	grub-mkrescue -o dos64.iso iso

shell/%.o: shell/%.cpp
	$(CXX) $(APPFLAGS) -c $< -o $@

hello.elf: shell/hello.o shell/shell.ld
	ld -m elf_x86_64 -T shell/shell.ld -nostdlib -o $@ shell/hello.o

gedit.elf: shell/gedit.o shell/shell.ld
	ld -m elf_x86_64 -T shell/shell.ld -nostdlib -o $@ shell/gedit.o

keyboard.sys: drv/keyb.o shell/drv/drv.ld
	ld -m elf_x86_64 -T shell/drv/drv.ld -nostdlib -o $@ shell/drv/keyb.o

clean:
	rm -f $(OBJS) dos64.bin dos64.iso shell/hello.o shell/gedit.o hello.elf gedit.elf
	rm -rf iso
