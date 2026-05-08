#include "std.h"

long long abs(long long x)              { return x < 0 ? -x : x; }
long long min(long long a, long long b) { return a < b ? a : b; }
long long max(long long a, long long b) { return a > b ? a : b; }


void itoa(long long n, char* buf) {
    if (n == 0) { buf[0]='0'; buf[1]='\0'; return; }
    bool neg = n < 0;
    if (neg) n = -n;
    char tmp[32]; int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    if (neg) tmp[i++] = '-';
    for (int j = 0; j < i; j++) buf[j] = tmp[i-1-j];
    buf[i] = '\0';
}

void utoa(unsigned long long n, char* buf) {
    if (n == 0) { buf[0]='0'; buf[1]='\0'; return; }
    char tmp[32]; int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    for (int j = 0; j < i; j++) buf[j] = tmp[i-1-j];
    buf[i] = '\0';
}

void htoa(unsigned long long n, char* buf) {
    const char* hex = "0123456789ABCDEF";
    buf[0]='0'; buf[1]='x';
    for (int i = 15; i >= 2; i--) {
        buf[i] = hex[n & 0xF];
        n >>= 4;
    }
    buf[18] = '\0';
}

long long atoi(const char* s) {
    long long result = 0;
    bool neg = false;
    if (*s == '-') { neg = true; s++; }
    while (*s >= '0' && *s <= '9')
        result = result * 10 + (*s++ - '0');
    return neg ? -result : result;
}