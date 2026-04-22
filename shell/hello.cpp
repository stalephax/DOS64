// Programme test — syscalls DOS64
#define SYS_PRINTLN 1
#define SYS_EXIT    4

static void sys_println(const char* s) {
    asm volatile(
        "mov $1, %%rax\n"    // SYS_PRINTLN
        "mov %0, %%rdi\n"    // argument = string
        "int $0x80\n"
        :: "r"(s) : "rax", "rdi"
    );
}

static void sys_exit(int code) {
    asm volatile(
        "mov $4, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"((long long)code) : "rax", "rdi"
    );
}

extern "C" int main() {
    sys_println("Hello from ELF64!");
    sys_println("Je suis un vrai programme.");
    sys_exit(0);
    return 0;
}