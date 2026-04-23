cd "C:\Program Files\qemu"
qemu-system-x86_64 -boot d -cdrom \\wsl$\Ubuntu\home\jerou\DOS64-1\dos64.iso -hda \\wsl$\Ubuntu\home\jerou\DOS64-1\disk.img -cpu qemu64 -m 256