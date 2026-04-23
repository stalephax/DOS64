#pragma once

// =============================================
// Structure multiboot memory map
// =============================================
struct MultibootMmapEntry {
    unsigned int  size;      // offset 0  — taille de l'entrée (sans ce champ)
    unsigned int  addr_low;  // offset 4
    unsigned int  addr_high; // offset 8
    unsigned int  len_low;   // offset 12
    unsigned int  len_high;  // offset 16
    unsigned int  type;      // offset 20
} __attribute__((packed));


struct MultibootInfo {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int syms[4];
    unsigned int mmap_length;
    unsigned int mmap_addr;
} __attribute__((packed));

// =============================================
// Gestionnaire mémoire physique (bitmap)
// =============================================
class MemorySystem {
    static const unsigned long long PAGE_SIZE = 4096;
    static const unsigned long long MAX_PAGES = (4ULL * 1024ULL * 1024ULL * 1024ULL) / PAGE_SIZE; // 4 Gio max

    // Bitmap : 1 bit par page (1 = libre, 0 = utilisé)
    unsigned char bitmap[MAX_PAGES / 8];
    unsigned long long total_pages = 0;
    unsigned long long free_pages  = 0;

    void set_used(unsigned long long page) {
    if (page >= MAX_PAGES) return;  // ← garde-fou
    bitmap[page / 8] &= ~(1 << (page % 8));
}

void set_free(unsigned long long page) {
    if (page >= MAX_PAGES) return;  // ← garde-fou
    bitmap[page / 8] |= (1 << (page % 8));
}


    bool is_free(unsigned long long page) {
        return bitmap[page / 8] & (1 << (page % 8));
    }

public:

    void init(MultibootInfo* mb, unsigned long long kernel_end) {
    // Tout marquer comme utilisé par défaut
    for (unsigned long long i = 0; i < MAX_PAGES / 8; i++)
        bitmap[i] = 0;

    unsigned long long mmap_addr = (unsigned long long)(unsigned int)mb->mmap_addr;
    unsigned long long mmap_end  = mmap_addr + mb->mmap_length;

    MultibootMmapEntry* entry = (MultibootMmapEntry*)mmap_addr;
    MultibootMmapEntry* end   = (MultibootMmapEntry*)mmap_end;

    while (entry < end) {
        unsigned long long addr = ((unsigned long long)entry->addr_high << 32) | entry->addr_low;
        unsigned long long len  = ((unsigned long long)entry->len_high  << 32) | entry->len_low;

        if (entry->type == 1) {
            unsigned long long start_page = addr / PAGE_SIZE;
            unsigned long long page_count = len  / PAGE_SIZE;

            for (unsigned long long i = 0; i < page_count; i++) {
                if (start_page + i < MAX_PAGES) {
                    set_free(start_page + i);
                    free_pages++;
                    total_pages++;
                }
            }
        }
        entry = (MultibootMmapEntry*)((unsigned char*)entry + entry->size + 4);
    }

    // Marquer tout ce qui est utilisé par le kernel comme occupé
    // Du début jusqu'à kernel_end aligné sur page
    unsigned long long kernel_pages = (kernel_end + PAGE_SIZE - 1) / PAGE_SIZE;
    for (unsigned int i = 0; i < kernel_pages; i++)
        set_used(i);

    // Marquer le premier Mo comme utilisé
    for (unsigned long long i = 0; i < 256; i++)
        set_used(i);
    }
        


    // Allouer une page physique (retourne l'adresse)
    void* alloc_page() {
        for (unsigned long long i = 256; i < MAX_PAGES; i++) {
            if (is_free(i)) {
                set_used(i);
                free_pages--;
                return (void*)(i * PAGE_SIZE);
            }
        }
        return nullptr;  // Plus de mémoire !
    }

    // Libérer une page
    void free_page(void* addr) {
        unsigned long long page = (unsigned long long)addr / PAGE_SIZE;
        if (page < MAX_PAGES && !is_free(page)) {
            set_free(page);
            free_pages++;
        }
    }

    // Allouer n pages contiguës
    void* alloc_pages(unsigned long long n) {
        unsigned long long count = 0;
        unsigned long long start = 0;

        for (unsigned long long i = 256; i < MAX_PAGES; i++) {
            if (is_free(i)) {
                if (count == 0) start = i;
                count++;
                if (count == n) {
                    for (unsigned long long j = start; j < start + n; j++)
                        set_used(j);
                    free_pages -= n;
                    return (void*)(start * PAGE_SIZE);
                }
            } else {
                count = 0;
            }
        }
        return nullptr;
    }

    unsigned long long get_free_mb() { return (free_pages * PAGE_SIZE) / (1024 * 1024); }
    unsigned long long get_total_mb() { return (total_pages * PAGE_SIZE) / (1024 * 1024); }
};

class HeapAllocator {
    unsigned long long heap_start;
    unsigned long long heap_end;
    unsigned long long current;

    struct Block {
        unsigned long long size;
        bool used;
        Block* next;
    };

    Block* head = nullptr;

public:
    void init(unsigned long long start, unsigned long long size) {
        heap_start = start;
        heap_end   = start + size;
        current    = start;
    }

    void* malloc(unsigned long long size) {
        // Aligner sur 8 octets
        size = (size + 7) & ~7;

        // Chercher un bloc libre existant
        Block* b = head;
        while (b) {
            if (!b->used && b->size >= size) {
                b->used = true;
                return (void*)((unsigned long long)b + sizeof(Block));
            }
            b = b->next;
        }

        // Nouveau bloc
        if (current + sizeof(Block) + size > heap_end)
            return nullptr;  // Heap pleine

        Block* newb = (Block*)current;
        newb->size = size;
        newb->used = true;
        newb->next = head;
        head = newb;
        current += sizeof(Block) + size;

        return (void*)((unsigned long long)newb + sizeof(Block));
    }

    void free(void* ptr) {
        if (!ptr) return;
        Block* b = (Block*)((unsigned long long)ptr - sizeof(Block));
        b->used = false;
    }
};