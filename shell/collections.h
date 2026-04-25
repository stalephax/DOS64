#pragma once
#include "../memory.h"
#include "std.h"

// ============================================================
// Array<T, N> — tableau statique de taille fixe
// Remplace : T arr[N] mais avec bounds checking
// ============================================================
template<typename T, int N>
class Array {
    T data[N];
    int _size = 0;

public:
    void push(const T& val) {
        if (_size < N) data[_size++] = val;
    }

    T& operator[](int i)       { return data[i]; }
    const T& operator[](int i) const { return data[i]; }

    int  size()  const { return _size; }
    int  capacity() const { return N; }
    bool full()  const { return _size >= N; }
    bool empty() const { return _size == 0; }

    void clear() { _size = 0; }

    void remove(int index) {
        if (index < 0 || index >= _size) return;
        for (int i = index; i < _size - 1; i++)
            data[i] = data[i + 1];
        _size--;
    }

    // Itération for-range
    T* begin() { return data; }
    T* end()   { return data + _size; }
};

// ============================================================
// Vector<T> — tableau dynamique (comme std::vector)
// Nécessite HeapAllocator
// ============================================================
template<typename T>
class Vector {
    T*   data     = nullptr;
    int  _size    = 0;
    int  _capacity = 0;
    HeapAllocator* heap;

    void grow() {
        int new_cap = _capacity == 0 ? 4 : _capacity * 2;
        T* new_data = (T*)heap->malloc(new_cap * sizeof(T));
        if (!new_data) return;
        for (int i = 0; i < _size; i++)
            new_data[i] = data[i];
        if (data) heap->free(data);
        data = new_data;
        _capacity = new_cap;
    }

public:
    Vector(HeapAllocator* h) : heap(h) {}

    ~Vector() { if (data) heap->free(data); }

    void push_back(const T& val) {
        if (_size >= _capacity) grow();
        if (_size < _capacity) data[_size++] = val;
    }

    void pop_back() {
        if (_size > 0) _size--;
    }

    T& operator[](int i)             { return data[i]; }
    const T& operator[](int i) const { return data[i]; }

    int  size()     const { return _size; }
    int  capacity() const { return _capacity; }
    bool empty()    const { return _size == 0; }

    void clear() { _size = 0; }

    void remove(int index) {
        if (index < 0 || index >= _size) return;
        for (int i = index; i < _size - 1; i++)
            data[i] = data[i + 1];
        _size--;
    }

    // Chercher un élément
    int find(const T& val) const {
        for (int i = 0; i < _size; i++)
            if (data[i] == val) return i;
        return -1;
    }

    T* begin() { return data; }
    T* end()   { return data + _size; }
};

// ============================================================
// String — chaîne de caractères dynamique
// Remplace : std::string
// ============================================================
class String {
    char*  data     = nullptr;
    int    _len     = 0;
    int    _capacity = 0;
    HeapAllocator* heap;

    void ensure(int needed) {
        if (needed < _capacity) return;
        int new_cap = needed + 16;
        char* new_data = (char*)heap->malloc(new_cap);
        if (!new_data) return;
        if (data) {
            memcpy(new_data, data, _len + 1);
            heap->free(data);
        }
        data = new_data;
        _capacity = new_cap;
    }

public:
    String(HeapAllocator* h) : heap(h) {
        ensure(16);
        if (data) data[0] = '\0';
    }

    String(HeapAllocator* h, const char* s) : heap(h) {
        int l = strlen(s);
        ensure(l + 1);
        if (data) {
            memcpy(data, s, l + 1);
            _len = l;
        }
    }

    ~String() { if (data) heap->free(data); }

    void append(const char* s) {
        int l = strlen(s);
        ensure(_len + l + 1);
        if (data) {
            memcpy(data + _len, s, l + 1);
            _len += l;
        }
    }

    void append(char c) {
        ensure(_len + 2);
        if (data) {
            data[_len++] = c;
            data[_len]   = '\0';
        }
    }

    void clear() {
        _len = 0;
        if (data) data[0] = '\0';
    }

    const char* c_str() const { return data ? data : ""; }
    int  length()       const { return _len; }
    bool empty()        const { return _len == 0; }

    char operator[](int i) const { return data ? data[i] : 0; }

    bool equals(const char* s) const {
        return strcmp(c_str(), s) == 0;
    }

    // Extraire une sous-chaîne
    void substr(int start, int len, char* out) const {
        if (!data || start >= _len) { out[0] = '\0'; return; }
        int l = len < _len - start ? len : _len - start;
        memcpy(out, data + start, l);
        out[l] = '\0';
    }
};

// ============================================================
// Pair<A, B> — paire de valeurs
// Remplace : std::pair
// ============================================================
template<typename A, typename B>
struct Pair {
    A first;
    B second;
    Pair() {}
    Pair(const A& a, const B& b) : first(a), second(b) {}
};

// ============================================================
// HashMap<K, V> — dictionnaire clé/valeur
// Implémentation simple par chaînage
// ============================================================
template<typename V, int BUCKETS = 64>
class HashMap {
    struct Entry {
        char key[64];
        V    value;
        bool used = false;
        Entry* next = nullptr;
    };

    Entry   buckets[BUCKETS];
    HeapAllocator* heap;

    int hash(const char* key) const {
        unsigned int h = 5381;
        while (*key) h = h * 33 ^ (unsigned char)*key++;
        return h % BUCKETS;
    }

public:
    HashMap(HeapAllocator* h) : heap(h) {}

    void set(const char* key, const V& val) {
        int idx = hash(key);
        Entry* e = &buckets[idx];

        // Chercher si la clé existe déjà
        while (e) {
            if (e->used && strcmp(e->key, key) == 0) {
                e->value = val;
                return;
            }
            if (!e->next) break;
            e = e->next;
        }

        // Nouvelle entrée
        if (!buckets[idx].used) {
            // Slot principal libre
            int i = 0;
            for (; key[i] && i < 63; i++) buckets[idx].key[i] = key[i];
            buckets[idx].key[i] = '\0';
            buckets[idx].value  = val;
            buckets[idx].used   = true;
        } else {
            // Collision — allouer un nouveau nœud
            Entry* newe = (Entry*)heap->malloc(sizeof(Entry));
            if (!newe) return;
            int i = 0;
            for (; key[i] && i < 63; i++) newe->key[i] = key[i];
            newe->key[i] = '\0';
            newe->value  = val;
            newe->used   = true;
            newe->next   = nullptr;
            e->next = newe;
        }
    }

    V* get(const char* key) {
        int idx = hash(key);
        Entry* e = &buckets[idx];
        while (e) {
            if (e->used && strcmp(e->key, key) == 0)
                return &e->value;
            e = e->next;
        }
        return nullptr;
    }

    bool contains(const char* key) {
        return get(key) != nullptr;
    }

    void remove(const char* key) {
        int idx = hash(key);
        Entry* e = &buckets[idx];
        if (e->used && strcmp(e->key, key) == 0) {
            e->used = false;
            return;
        }
        while (e->next) {
            if (strcmp(e->next->key, key) == 0) {
                Entry* dead = e->next;
                e->next = dead->next;
                heap->free(dead);
                return;
            }
            e = e->next;
        }
    }
};

// ============================================================
// Optional<T> — valeur optionnelle
// Remplace : std::optional
// ============================================================
template<typename T>
class Optional {
    T    value;
    bool _has = false;

public:
    Optional() : _has(false) {}
    Optional(const T& v) : value(v), _has(true) {}

    bool has_value() const { return _has; }
    T&   get()             { return value; }
    const T& get() const   { return value; }

    void set(const T& v) { value = v; _has = true; }
    void reset()         { _has = false; }
};