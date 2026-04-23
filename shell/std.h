// Programme test — syscalls DOS64
#define SYS_PRINT   0
#define SYS_PRINTLN 1
#define SYS_MALLOC  2
#define SYS_FREE    3
#define SYS_EXIT    4
#define SYS_GETCHAR 5
#define SYS_GETLINE 6


static void write(const char* s) {
    asm volatile(
        "mov $0, %%rax\n"    // SYS_PRINT
        "mov %0, %%rdi\n"    // argument = string
        "int $0x80\n"
        :: "r"(s) : "rax", "rdi"
    );
}



static void writeLn(const char* s) {
    asm volatile(
        "mov $1, %%rax\n"    // SYS_PRINTLN
        "mov %0, %%rdi\n"    // argument = string
        "int $0x80\n"
        :: "r"(s) : "rax", "rdi"
    );
}

static void suicide(int code) {
    asm volatile(
        "mov $4, %%rax\n"
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"((long long)code) : "rax", "rdi"
    );
}

static void alloc(long size) {
    asm volatile(
        "mov $2, %%rax\n" // SYS_MALLOC
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"(size) : "rax", "rdi"
    );
}

static void free(long size) {
    asm volatile(
        "mov $3, %%rax\n" // SYS_FREE
        "mov %0, %%rdi\n"
        "int $0x80\n"
        :: "r"(size) : "rax", "rdi"
    );

}