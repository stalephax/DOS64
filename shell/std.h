// Programme test — syscalls DOS64
#define SYS_PRINT   0
#define SYS_PRINTLN 1
#define SYS_MALLOC  2
#define SYS_FREE    3
#define SYS_EXIT    4
#define SYS_GETCHAR 5
#define SYS_GETLINE 6
#define SYS_RUN     7
#define SYS_DIR     8
#define SYS_CD      9
#define SYS_TYPE    10
#define SYS_MKDIR   11
#define SYS_RENAME  12
#define SYS_DEL     13
#define SYS_POWEROFF 14
#define SYS_REBOOT   15
#define SYS_GETCWD   16
#define SYS_LS       17
#define SYS_OPEN     18
#define SYS_CLOSE    19
#define SYS_READ     20
#define SYS_WRITE    21
#define SYS_SEEK     22
#define SYS_TELL     23
#define SYS_FILESIZE 24
#define SYS_GFXMODE  25
#define GFX_WIDTH 800
#define GFX_HEIGHT 600
#define GFX_DRAWPIX 102400


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