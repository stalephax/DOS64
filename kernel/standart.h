#pragma once

// Fonctions de manipulation de mémoire
// définies dans standart.cpp pour éviter les dépendances circulaires avec memory.cpp
extern "C" void* memset(void* ptr, int value, unsigned long long size);
extern "C" void* memcpy(void* dst, const void* src, unsigned long long size);

// Fonctions mathématiques de base
static inline unsigned long long abs(long long x) {
    return x < 0 ? -x : x;
}

// Fonctions de manipulation de chaînes

// strcmp : compare deux chaînes, retourne 0 si égales, <0 si a < b, >0 si a > b
static inline int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

// strncmp : compare les n premiers caractères de deux chaînes
static inline int strncmp(const char* a, const char* b, int n) {
    while (n-- && *a && *a == *b) { a++; b++; }
    return n < 0 ? 0 : *a - *b;
}

// strlen : retourne la longueur d'une chaîne
static inline int strlen(const char* s) {
    int i = 0;
    while (s[i]) i++;
    return i;
}

// ulltoa : convertit un entier non signé en chaîne décimale
static inline void ulltoa(unsigned long long n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    int i = 0;
    char tmp[32];
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    for (int j = 0; j < i; j++) buf[j] = tmp[i - 1 - j];
    buf[i] = '\0';
}