#include "standart.h"
// fonction de base pour les opérations de mémoire, comme memset et memcpy, qui sont utilisées par d'autres parties du kernel, notamment le système de mémoire et le gestionnaire de fichiers, pour initialiser des structures de données ou copier des blocs de mémoire
extern "C" void* memset(void* ptr, int value, unsigned long long size) {
    unsigned char* p = (unsigned char*)ptr;
    while (size--) *p++ = (unsigned char)value;
    return ptr;
}

extern "C" void* memcpy(void* dst, const void* src, unsigned long long size) {
    unsigned char* d = (unsigned char*)dst;
    const unsigned char* s = (const unsigned char*)src;
    while (size--) *d++ = *s++;
    return dst;
}