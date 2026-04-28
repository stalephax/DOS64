#include "std.h"

int strlen(const char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char* a, const char* b, int n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    return n < 0 ? 0 : *a - *b;
}

char* strcpy(char* dst, const char* src) {
    int i = 0;
    while ((dst[i] = src[i])) i++;
    return dst;
}

char* strcat(char* dst, const char* src) {
    int i = strlen(dst);
    int j = 0;
    while ((dst[i++] = src[j++]));
    return dst;
}

char* strchr(const char* s, char c) {
    for (; *s; s++)
        if (*s == c) return (char*)s;
    return nullptr;
}

void* memset(void* ptr, int val, unsigned long long size) {
    unsigned char* p = (unsigned char*)ptr;
    while (size--) *p++ = (unsigned char)val;
    return ptr;
}

void* memcpy(void* dst, const void* src, unsigned long long size) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (size--) *d++ = *s++;
    return dst;
}