#include "drivers/keyboard.h"
#include "prompt.h"
#include "standart.h"
#include "memory.h"
#include "panic.h" // ça aidera pour le débug et les erreurs critiques, même si pour l'instant on s'en sert pas beaucoup
#include "drivers/video.h"
#include "drivers/mouse.h" // pour les tests de souris, même remarque que pour FAT32.h
#include "elf64.h" // pour les tests de chargement d'ELF, même remarque que pour FAT32.h
#include "idt.h" // pour les tests d'interruptions, même remarque que pour FAT32.h
#include "drivers/beep.h" // pour les tests de son, même remarque que pour FAT32.h
#include "drivers/acpi.h" // pour les tests d'ACPI, même remarque que pour FAT32.h
#include "vmouse.h"
#include "fs/vfs.h"
#include "fs/vdrive.h"      // 1. interface abstraite
#include "drivers/ata.h"    // 2. driver ATA
#include "drivers/ramdrv.h" // 3. driver RAM
#include "drivers/ata_lf.h"      // 4. adapteur ATA
#include "drivers/ram_lf.h"      // 5. adapteur RAM
#include "fs/FAT32.h"       // 6. filesystem (utilise DiskDrive*)
/*
* KERNEL DOS64
*   mis à jour le 22/04/2026
*      
*   supporte : clavier I/O, 
* VGA mode texte basique, 
* Terminal, 
* lecture I/O disque ATA
* système de mémoire avec bitmap et gestion de heap simple
*
*
*/









inline void* operator new(unsigned long, void* p) { return p; }
// Adresse mémoire du buffer VGA (texte mode)
static unsigned short* const T_VGA = (unsigned short*)0xB8000;

static unsigned char term_buf[sizeof(Terminal)];
static unsigned char kbd_buf[sizeof(Keyboard)];
static unsigned char mm_buf[sizeof(MemorySystem)];
static unsigned char heap_buf[sizeof(HeapAllocator)];
static unsigned char ata_buf[sizeof(ATADriver)];
static unsigned char fat_buf[sizeof(FAT32)];
static unsigned char elf_buf[sizeof(ELF64Loader)];
static unsigned char idt_buf[sizeof(IDT)];
static unsigned char vga_buf[sizeof(VGAGraphics)];
static unsigned char beep_buf[sizeof(Beeper)];
static unsigned char mouse_buf[sizeof(PS2Mouse)];
static unsigned char acpi_buf[sizeof(ACPI)];
static unsigned char cursor_buf[sizeof(MouseCursor)];
static unsigned char path_buf[sizeof(PathManager)];
static unsigned char ramdisk_buf[RAMDISK_SIZE];   // 1 Mo en BSS
static unsigned char ram_buf[sizeof(RamDisk)];
static unsigned char ramfs_buf[sizeof(FAT32)];
static unsigned char ata_if_buf[sizeof(ATADiskIF)];
static unsigned char ram_if_buf[sizeof(RamDiskIF)];


PathManager* pm;
MouseCursor* cursor;
Terminal* term;
Keyboard* kbd;
MemorySystem* mm;
HeapAllocator* heap;
ATADriver* ata;
IDT* idt;
FAT32* fs; // système de fichiers FAT32, pour les tests de lecture de disque
ELF64Loader* elf_64; // pour les tests de chargement d'ELF
VGAGraphics* vga; // pour les tests de vidéo
Beeper* beep; // pour les tests de son
PS2Mouse* mouse; // pour les tests de souris
ACPI* acpi; // pour les tests d'ACPI, même remarque que pour FAT32.h
RamDisk* ramdisk;
FAT32*   ramfs;
ATADiskIF* ata_if;
RamDiskIF* ram_if;
bool power = false; // pour le moment on peut pas éteindre la machine, par manque de prise en charge de l'ACPI, mais ça peut servir pour un reboot ou une mise en veille plus tard

// SVP ne pas enlever cette méthode ou je vous tue.
extern "C" VGAGraphics* ensure_vga() {
    if (!vga) vga = new (vga_buf) VGAGraphics;
    return vga;
}

// Palette de couleurs VGA 4 bit

static const unsigned char BLACK = 0x0;
static const unsigned char BLUE = 0x1;
static const unsigned char GREEN = 0x2;
static const unsigned char CYAN = 0x3;
static const unsigned char RED = 0x4;
static const unsigned char MAGENTA = 0x5;
static const unsigned char BROWN = 0x6;
static const unsigned char LIGHT = 0x7;
static const unsigned char GRAY = 0x8;
static const unsigned char LIGHT_BLUE = 0x9;
static const unsigned char LIGHT_GREEN = 0xA;
static const unsigned char LIGHT_CYAN = 0xB;
static const unsigned char LIGHT_RED = 0xC;
static const unsigned char LIGHT_MAGENTA = 0xD;
static const unsigned char YELLOW = 0xE;
static const unsigned char WHITE = 0xF;

// méthodes à déclarer

static FAT32* current_fs();


constexpr unsigned char t_color(unsigned char fg, unsigned char bg = BLACK) {
    return (bg << 4) | fg;
}

void print(const char* str, int line = 0, unsigned char color = 0x0F) {
    for (int i = 0; str[i] != '\0'; i++) {
        T_VGA[line * 80 + i] = (unsigned short)str[i] | (color << 8);
    }
}

void mount_ata(); // déclaration de la fonction d'initialisation du disque, définie plus bas, pour éviter les problèmes d'ordre d'initialisation des objets globaux

void delay (const int d) // delai décimal simple
{
    for (volatile int c = 0; c < d ; c++ )  ; // ne fait rien entre ces deux
}

extern unsigned char _kernel_end[];

// ============================================================
// INIT — Initialisation séquentielle et sécurisée de DOS64
// Chaque étape est indépendante et affiche son statut
// ============================================================

// Helper : afficher le statut d'une étape sur une ligne fixe
static void init_status(const char* msg, int line, bool ok) {
    print(msg, line, t_color(ok ? LIGHT_GREEN : LIGHT_RED));
}

// Helper : afficher une valeur numérique de debug
static void init_debug(unsigned long long val, int line, int col) {
    char buf[32];
    ulltoa(val, buf);
    for (int i = 0; buf[i] && col + i < 80; i++)
        T_VGA[line * 80 + col + i] = (unsigned short)buf[i] | (t_color(YELLOW) << 8);
}

void init(unsigned long long mb_addr) {

    // --------------------------------------------------------
    // PHASE 0 : Banner — avant tout driver
    // --------------------------------------------------------
    for (int i = 0; i < 80 * 25; i++)
        T_VGA[i] = (t_color(WHITE, BLACK) << 8) | ' ';

    print("DOS64 Boot Sequence", 0, t_color(LIGHT_CYAN));
    print("--------------------", 1, t_color(GRAY));
    power = true;

    // --------------------------------------------------------
    // PHASE 1 : Terminal (requis pour afficher quoi que ce soit après)
    // --------------------------------------------------------
    term = new (term_buf) Terminal;
    init_status("Terminal        [ OK ]", 2, true);

    // --------------------------------------------------------
    // PHASE 2 : Clavier
    // --------------------------------------------------------
    kbd = new (kbd_buf) Keyboard;
    init_status("Keyboard        [ OK ]", 3, true);

    // --------------------------------------------------------
    // PHASE 3 : Mémoire physique
    // --------------------------------------------------------
    if (mb_addr == 0 || mb_addr > 0x3FFFFFFF) {
        init_status("Memory          [FAIL] bad mb_addr", 4, false);
        asm volatile("cli; hlt");
    }
    mm = new (mm_buf) MemorySystem;
    MultibootInfo* mb = (MultibootInfo*)mb_addr;

    // Vérifier le flag mmap (bit 6)
    if (!(mb->flags & (1 << 6))) {
        init_status("Memory          [FAIL] no mmap", 4, false);
        asm volatile("cli; hlt");
    }
    mm->init(mb, (unsigned long long)_kernel_end);
    init_status("Memory          [ OK ]", 4, true);
    init_debug(mm->get_free_mb(), 4, 50);

    // --------------------------------------------------------
    // PHASE 4 : Heap
    // --------------------------------------------------------
    heap = new (heap_buf) HeapAllocator;
    unsigned long long heap_start = ((unsigned long long)_kernel_end + 4095) & ~4095;
    heap->init(heap_start, 4 * 1024 * 1024);
    init_status("Heap            [ OK ]", 5, true);
    init_debug(heap_start, 5, 50);

    // --------------------------------------------------------
    // PHASE 5 : IDT + interruptions
    // --------------------------------------------------------
    idt = new (idt_buf) IDT;
    idt->init();
    init_status("IDT             [ OK ]", 6, true);

    // --------------------------------------------------------
    // PHASE 6 : ELF64 Loader
    // --------------------------------------------------------
    elf_64 = new (elf_buf) ELF64Loader(heap);
    init_status("ELF64 Loader    [ OK ]", 7, true);

    // --------------------------------------------------------
    // PHASE 7 : ACPI (requis pour shutdown/reboot)
    // --------------------------------------------------------
    acpi = new (acpi_buf) ACPI;
    acpi->init();
    init_status("ACPI            [ OK ]", 8, true);

    // --------------------------------------------------------
    // PHASE 8 : Souris + curseur
    // --------------------------------------------------------
    mouse = new (mouse_buf) PS2Mouse;
    bool mouse_ok = mouse->init();
    init_status("PS2 Mouse       [ OK ]", 9, mouse_ok);
    cursor = new (cursor_buf) MouseCursor;

    // --------------------------------------------------------
    // PHASE 9 : Path Manager (VFS minimal)
    // --------------------------------------------------------
    pm = new (path_buf) PathManager;
    pm->init();
    init_status("Path Manager    [ OK ]", 10, true);

    // --------------------------------------------------------
    // PHASE 10 : RAM disk A:
    // — seulement si la heap est assez grande
    // --------------------------------------------------------
    ramdisk = new (ram_buf) RamDisk;
    bool ram_ok = ramdisk->init(ramdisk_buf, RAMDISK_SIZE);

    if (ram_ok) {
        bool fmt_ok = RamDiskFormatter::format(ramdisk);
        if (fmt_ok) {
            ramfs = new (ramfs_buf) FAT32;
            RamDiskIF* rif = new (ram_if_buf) RamDiskIF(ramdisk);
            bool mnt_ok = ramfs->mount((DiskDrive*)rif);
            init_status("RAM Disk A:     [ OK ]", 11, mnt_ok);
            if (!mnt_ok) ramfs = nullptr;
        } else {
            init_status("RAM Disk A:     [FAIL] format", 11, false);
            ramfs = nullptr;
        }
    } else {
        init_status("RAM Disk A:     [FAIL] init", 11, false);
        ramfs = nullptr;
    }

    // --------------------------------------------------------
    // PHASE 11 : Disque ATA + FAT32 C:
    // — pas fatal si absent
    // --------------------------------------------------------
    ata = new (ata_buf) ATADriver;
    bool ata_ok = ata->init();

    if (ata_ok && ata->is_present()) {
        init_status("ATA Disk        [ OK ]", 12, true);

        ata_if = new (ata_if_buf) ATADiskIF(ata);
        fs = new (fat_buf) FAT32;
        bool fs_ok = fs->mount((DiskDrive*)ata_if);

        if (fs_ok) {
            init_status("FAT32 C:        [ OK ]", 13, true);
        } else {
            init_status("FAT32 C:        [FAIL] not formatted?", 13, false);
            fs = nullptr;
        }
    } else {
        init_status("ATA Disk        [----] not found", 12, false);
        init_status("FAT32 C:        [----] no disk", 13, false);
        ata = nullptr;
        fs  = nullptr;
    }

    // --------------------------------------------------------
    // PHASE 12 : Beeper — en dernier (signal sonore de fin)
    // --------------------------------------------------------
    beep = new (beep_buf) Beeper;
    beep->beep_boot();
    init_status("Beeper          [ OK ]", 14, true);

    // --------------------------------------------------------
    // FIN — pause courte puis on démarre le shell
    // --------------------------------------------------------
    print("Boot complete. Starting shell...", 16, t_color(WHITE));
    delay(15000000);
}
void mount_ata() { 
    term->println("Initializing ATA Driver...");
    delay(90000000);
    term->println("trying to create the Driver Object...");
    delay(90000000);
    // ça s'arrête là.. étrange
    // je crois le driver ATA overflow le BSS? 
    // c'est l'objet de trop, ou il est trop lourd?
    // ça explique rien mais la spéculation peut révéler des choses
    ata = new (ata_buf) ATADriver; // IL FALLAIT ENLEVER LES PARENTHESES, WTF?
    // taille de ata_buf = 42 octets -> NORMAL
    delay(90000000);
    delay(90000000);

    // Vérifier l'allocation avant tout
    // Quand le driver n'est pas crée, il fait cet écran d'erreur classique
    if (!ata) {
        kernel_panic("ATA Driver alloc failed", 0x001);
        return;
    }
    term->println("driver Object Created");
    delay(90000000);
    delay(90000000);
    term->println("trying to init... ");
    delay(90000000);delay(90000000);
    // Tenter l'init — ne pas paniquer si pas de disque
    bool ok = ata->init();
    delay(90000000);delay(90000000);
    if (!ok || !ata->is_present()) {
        term->println("No disk found.");
        return;  // pas fatal
    }
    delay(90000000);delay(90000000);
    term->print("Disk: ");
    term->println(ata->get_model());
    term->println("ATA driver initialized.");
}
// --- INTERPRETEUR DE COMMANDES ---
// ============================================================
// INTERPRÉTEUR DE COMMANDES — DOS64
// ============================================================
// Helpers internes
// ============================================================

// Vérifie si un fichier existe dans le volume courant (A: ou C:)
static bool fat_file_exists(const char* path) {
    FAT32* cur = current_fs();
    if (!cur || !cur->is_mounted()) return false;
    FAT32_File f = cur->open(path);
    return f.valid;
}

// Vérifie que le filesystem est monté, affiche un message sinon
static bool require_fs() {
    if (!fs || !fs->is_mounted()) {
        term->println("No filesystem mounted. Use MOUNT first.");
        return false;
    }
    return true;
}

// Vérifie qu'un argument est présent après une commande
static bool require_arg(const char* cmd, int min_len) {
    if ((int)strlen(cmd) <= min_len) {
        term->println("Missing argument.");
        return false;
    }
    return true;
}

// ============================================================
// Commandes — Système
// ============================================================

static void cmd_help() {
    term->set_color(LIGHT_CYAN);
    term->println("=== DOS64 Command Reference ===");
    term->set_color(WHITE);
    term->println("");

    term->set_color(YELLOW);  term->println("-- System --");
    term->set_color(WHITE);
    term->println("  HELP              Show this message");
    term->println("  CLEAR             Clear the screen");
    term->println("  MEM               Show memory usage");
    term->println("  ECHO [text]       Print text to screen");
    term->println("  SHUTUP [r]        Shutdown (or reboot with 'r')");
    term->println("  REBOOT            Reboot the machine");

    term->set_color(YELLOW);  term->println("");
    term->set_color(YELLOW);  term->println("-- Disk & Filesystem --");
    term->set_color(WHITE);
    term->println("  VOLS              List available volumes");
    term->println("  MOUNT             Initialize ATA driver and mount C:");
    term->println("  DIR [path]        List files in current or given directory");
    term->println("  CD [path]         Change current directory (CD .. to go up)");
    term->println("  TYPE [file]       Display contents of a text file");
    term->println("  MKDIR [name]      Create a new directory");
    term->println("  RUN [file.elf]    Load and execute a 64-bit ELF binary");
    term->println("  [file(.elf)]      Execute ELF directly by typing its name");

    term->set_color(YELLOW);  term->println("");
    term->set_color(YELLOW);  term->println("-- Debug --");
    term->set_color(WHITE);
    term->println("  LAXATIVE          Dump memory map");
    term->println("  WRAW [data]       Write raw data to disk sector 0");
    term->println("  FKUP              Trigger kernel panic (test)");
    term->println("  GFX               Test graphics mode");
    term->set_color(WHITE);
}
/*
static void cmd_diskpart(ATADriver* ata) { // partitionne en FAT32, si au cas ou un disque ne serais pas formaté, ça peut être utile pour les tests, même si c'est pas une priorité
    if (!ata || !ata->is_present()) {
        term->println("No disk found.");
        return;
    }
    if (!require_fs()) return;
    term->set_color(LIGHT_CYAN);
    term->println("=== Disk Partition Info ===");
    term->set_color(WHITE);
    fs->mount(ata); // Remonter pour être sûr d'avoir les infos à jour
}*/


static void cmd_clear() {
    term->clear();
}

static void cmd_mem() {
    char buf[32];
    term->set_color(LIGHT_CYAN);
    term->println("=== Memory Status ===");
    term->set_color(WHITE);

    term->print("  Free  : ");
    ulltoa(mm->get_free_mb(), buf);
    term->print(buf);
    term->println(" MB");

    term->print("  Total : ");
    ulltoa(mm->get_total_mb(), buf);
    term->print(buf);
    term->println(" MB");
}

static void cmd_echo(const char* text) {
    term->println(text);
}
static void cmd_rename(const char* args) {
    if (!require_fs()) return;

    // Parser "oldname newname"
    char old_name[256], new_name[256];
    int i = 0;
    while (args[i] && args[i] != ' ') { old_name[i] = args[i]; i++; }
    old_name[i] = '\0';
    if (!args[i]) { term->println("Usage: RENAME oldname newname"); return; }
    i++;
    int j = 0;
    while (args[i]) { new_name[j++] = args[i++]; }
    new_name[j] = '\0';

    char resolved[256];
    pm->resolve(old_name, resolved);

    if (fs->rename(resolved, new_name)) {
        term->print(old_name);
        term->print(" -> ");
        term->println(new_name);
    } else {
        term->println("Rename failed.");
    }
}

static void cmd_del(const char* path) {
    if (!require_fs()) return;
    if (!require_arg(path, 0)) return;

    char resolved[256];
    pm->resolve(path, resolved);
    if (fs->get_attributes(resolved) == -1) {
        term->print("THIS IS A DIRECTORY: ");
        term->println(path);
        return;
    }
    if (fs->remove(resolved)) {
        term->print("Deleted: ");
        term->println(path);
    } else {
        term->println("Delete failed. File may not exist or is a directory.");
    }
}
static void cmd_shutdown(char bootmode) {
 bool reboot = (bootmode == 'r' || bootmode == 'R');
    if (reboot) {
        term->set_color(BLUE);
        term->println("Rebooting...");
        delay(90000000);
        acpi->reboot();
    } else {
        term->set_color(RED);
        term->println("Shutting down...");
        delay(90000000);
        term->clear();
        print("  It is now safe to turn off your computer.", 0, t_color(YELLOW));
        power = false;
        delay(90000000);
        delay(90000000);
        delay(90000000);
        acpi->shutdown();
    }
}

// ============================================================
// Commandes — Filesystem
// ============================================================

static void cmd_mount() {
    mount_ata();
    if (ata && ata->is_present()) {
        // Monter le filesystem
        fs = new (fat_buf) FAT32;
        if (fs->mount(ata_if)) {
            term->set_color(LIGHT_GREEN);
            term->println("FAT32 mounted on C:");
            term->set_color(WHITE);
        } else {
            term->set_color(LIGHT_RED);
            term->println("FAT32 mount failed. Disk may not be formatted.");
            term->set_color(WHITE);
        }
    }
}

static void cmd_dir(const char* arg) {
    FAT32* cur = current_fs();  // ← volume courant
    if (!cur || !cur->is_mounted()) {
        term->println("No filesystem mounted. Use MOUNT first.");
        return;
    }

    char resolved[256];
    if (arg && arg[0]) {
        pm->resolve(arg, resolved);
    } else {
        // Lister le répertoire courant
        const char* cwd = pm->get_cwd();
        if (cwd[0]) {
            int i = 0;
            for (; cwd[i]; i++) resolved[i] = cwd[i];
            resolved[i] = '\0';
        } else {
            resolved[0] = '\0';  // racine
        }
    }

    cur->list_files(term, resolved);  // ← cur au lieu de fs
}

static void cmd_cd(const char* path) {
    if (!require_fs()) return;

    if (!path || !path[0]) {
        term->print(pm->get_volume());
        term->print(":/");
        term->println(pm->get_cwd());
        return;
    }

    // Changement de volume : "A:" ou "C:"
    if (path[1] == ':' && path[2] == '\0') {
        char vol = path[0];
        if ((vol == 'A' || vol == 'a') && ramfs) {
            pm->cd(path); return;
        }
        if ((vol == 'C' || vol == 'c') && fs) {
            pm->cd(path); return;
        }
        term->print("Volume not available: ");
        term->println(path);
        return;
    }

    // Navigation spéciale — jamais vérifiée dans FAT32
    if (strcmp(path, "..") == 0 ||
        strcmp(path, ".") == 0  ||
        strcmp(path, "/") == 0  ||
        strcmp(path, "\\") == 0) {
        pm->cd(path);
        return;
    }

    // Vérifier que la cible est bien un dossier dans FAT32
    char resolved[256];
    pm->resolve(path, resolved);

    FAT32* cur_fs = current_fs();
    if (!cur_fs || !cur_fs->is_directory(resolved)) {
        term->print("Not a directory: ");
        term->println(path);
        return;
    }

    pm->cd(path);
}

static FAT32* current_fs() {
    char vol = pm->get_volume()[0];
    if (vol == 'A' || vol == 'a') return ramfs;
    return fs;  // C: par défaut
}

static void cmd_type(const char* path) {
    if (!require_fs()) return;
    if (!require_arg(path, 0)) return;
    FAT32* cur = current_fs();
    if (!cur || !cur->is_mounted()) {
        term->println("No filesystem mounted.");
        return;
    }

    char resolved[256];
    pm->resolve(path, resolved);

    FAT32_File f = cur->open(resolved);
    if (!f.valid) {
        term->print("File not found: ");
        term->println(path);
        return;
    }

    static unsigned char file_buf[4096];
    unsigned int read_size = f.file_size < 4095 ? f.file_size : 4095;

    if (cur->read_file(&f, file_buf, read_size)) {
        file_buf[read_size] = '\0';
        term->println((char*)file_buf);
    } else {
        term->println("Error reading file.");
    }
}
static void cmd_vols() {
    term->set_color(LIGHT_CYAN);
    term->println("=== Available Volumes ===");
    term->set_color(WHITE);

    term->print("  A: ");
    if (ramdisk && ramdisk->is_present()) {
        term->print("[RAM DISK]  ");
        char sz[16];
        ulltoa(RAMDISK_SIZE / 1024, sz);
        term->print(sz);
        term->println(" KB");
    } else {
        term->println("[Not available]");
    }

    term->print("  C: ");
    if (fs && fs->is_mounted()) {
        term->print("[FAT32 HDD] ");
        if (ata) term->println(ata->get_model());
        else term->println("");
    } else {
        term->println("[Not mounted]");
    }
}
static void cmd_mkdir(const char* name) {
    if (!require_fs()) return;
    if (!require_arg(name, 0)) return;
    FAT32* cur = current_fs();
    if (!cur || !cur->is_mounted()) {  }

    char resolved[256];
    pm->resolve(name, resolved);

    if (cur->mkdir(resolved)) {
        term->print("Directory created: ");
        term->println(name);
    } else {
        term->println("Failed to create directory.");
        term->println("(Directory may already exist or disk is full)");
    }
}

static void cmd_run(const char* path) {
    if (!require_fs()) return;
    if (!require_arg(path, 0)) return;

    // Compléter avec .ELF si pas d'extension
    char full_path[260];
    int len = strlen(path);
    bool has_ext = (len > 4 && path[len-4] == '.');

    if (!has_ext) {
        int i = 0;
        for (; path[i]; i++) full_path[i] = path[i];
        full_path[i++] = '.';
        full_path[i++] = 'E';
        full_path[i++] = 'L';
        full_path[i++] = 'F';
        full_path[i]   = '\0';
        path = full_path;
    }

    char resolved[256];
    pm->resolve(path, resolved);

    FAT32* cur = current_fs();
    FAT32_File f = cur->open(resolved);
    if (!f.valid) {
        term->print("File not found: ");
        term->println(path);
        return;
    }

    unsigned char* buf = (unsigned char*)heap->malloc(f.file_size);
    if (!buf) {
        term->println("Not enough memory to load program.");
        return;
    }

    cur->read_file(&f, buf, f.file_size);
    int code = elf_64->load_and_run(buf, f.file_size);
    heap->free(buf);

    switch (code) {
        case -1: term->println("Error: not a valid ELF file."); break;
        case -2: term->println("Error: not an executable ELF."); break;
        case -3: term->println("Error: not x86_64 architecture."); break;
        default: {
            char ret[16];
            ulltoa((unsigned long long)code, ret);
            term->print("Process exited with code: ");
            term->println(ret);
        }
    }
}

// ============================================================
// Commandes — Debug
// ============================================================

static void cmd_laxative() {
    term->set_color(LIGHT_CYAN);
    term->println("=== Memory Map ===");
    term->set_color(WHITE);

    // Note : adresse approximative, à améliorer avec VFS
    MultibootMmapEntry* entry = (MultibootMmapEntry*)(
        (unsigned long long)mm->get_total_mb() * 1024 * 1024);
    MultibootMmapEntry* end = (MultibootMmapEntry*)(
        (unsigned long long)(mm->get_free_mb() + mm->get_total_mb()) * 1024 * 1024);

    while (entry < end) {
        char buf[32];
        term->print("  Base: 0x");
        ulltoa(entry->addr_high, buf); term->print(buf);
        term->print("  Len: 0x");
        ulltoa(entry->len_high, buf);  term->print(buf);
        term->print("  ");
        term->println(entry->type == 1 ? "[RAM]" : "[Reserved]");
        entry = (MultibootMmapEntry*)((unsigned char*)entry + entry->size + 4);
    }
}

static void cmd_wraw(const char* data) {
    if (!ata || !ata->is_present()) {
        term->println("No disk. Use MOUNT first.");
        return;
    }
    if (ata->write(0, 1, data))
        term->println("Write succeeded.");
    else
        term->println("Write failed.");
}

static void cmd_gfx() {
    vga = new (vga_buf) VGAGraphics;
    vga->init();
    vga->clear(VGAGraphics::BLACK);
    vga->fill_rect(10, 10, 100, 80, VGAGraphics::RED);
    vga->draw_line(0, 0, 319, 199, VGAGraphics::WHITE);
    term->println("GFX mode active. Restart to return to text mode.");
}

// ============================================================
// Dispatch principal
// ============================================================

void interpret_command(const char* cmd) {

    // Recherche d'abord un programme dans le répertoire courant.
    // Si trouvé, on l'exécute directement (avec ou sans extension ELF).
    char resolved[256];
    char token[260];
    char token_with_ext[260];
    token[0] = '\0';
    token_with_ext[0] = '\0';

    // On ne tente l'auto-exécution que sur un nom simple (sans arguments).
    int t = 0;
    while (cmd[t] && cmd[t] != ' ' && t < 259) {
        token[t] = cmd[t];
        t++;
    }
    token[t] = '\0';

    if (token[0] && cmd[t] == '\0') {
        // Essai 1 : nom exact (ex: hello.elf)
        pm->resolve(token, resolved);
        bool found = fat_file_exists(resolved);

        // Essai 2 : ajouter .ELF automatiquement (ex: hello -> hello.ELF)
        if (!found) {
            int i = 0;
            for (; token[i] && i < 255; i++) token_with_ext[i] = token[i];
            token_with_ext[i++] = '.';
            token_with_ext[i++] = 'E';
            token_with_ext[i++] = 'L';
            token_with_ext[i++] = 'F';
            token_with_ext[i] = '\0';

            pm->resolve(token_with_ext, resolved);
            found = fat_file_exists(resolved);
        }

        if (found) {
            cmd_run(token_with_ext[0] ? token_with_ext : token);
            return;
        }
    }

    // --- Système ---
    if      (strcmp(cmd, "help") == 0)              cmd_help();
    else if (strcmp(cmd, "clear") == 0)             cmd_clear();
    else if (strcmp(cmd, "mem") == 0)               cmd_mem();
    else if (strncmp(cmd, "echo ", 5) == 0)         cmd_echo(cmd + 5);
    else if (strcmp(cmd, "shutup") == 0)            cmd_shutdown('s');
    else if (strncmp(cmd, "shutup r", 8) == 0)      cmd_shutdown('r');
    else if (strcmp(cmd, "reboot") == 0)            cmd_shutdown('r');

    // --- Filesystem ---
    else if (strcmp(cmd, "vols") == 0)              cmd_vols();
    else if (strcmp(cmd, "mount") == 0)             cmd_mount();
    else if (strcmp(cmd, "dir") == 0)               cmd_dir(nullptr);
    else if (strncmp(cmd, "dir ", 4) == 0)          cmd_dir(cmd + 4);
    else if (strcmp(cmd, "cd") == 0)                cmd_cd(nullptr);
    else if (strncmp(cmd, "cd ", 3) == 0)           cmd_cd(cmd + 3);
    else if (strncmp(cmd, "type ", 5) == 0)         cmd_type(cmd + 5);
    else if (strncmp(cmd, "mkdir ", 6) == 0)        cmd_mkdir(cmd + 6);
    else if (strncmp(cmd, "run ", 4) == 0)          cmd_run(cmd + 4);
    // Dans le dispatch :
    else if (strncmp(cmd, "rename ", 7) == 0)   cmd_rename(cmd + 7);
    else if (strncmp(cmd, "del ", 4) == 0)      cmd_del(cmd + 4);
    //else if (strncmp(cmd, "copy ", 5) == 0)     cmd_copy(cmd + 5); unused
    // --- Debug ---
    else if (strcmp(cmd, "laxative") == 0)          cmd_laxative();
    else if (strncmp(cmd, "wraw ", 5) == 0)         cmd_wraw(cmd + 5);
    else if (strcmp(cmd, "fkup") == 0)              kernel_panic("DEBUG TESTING", 0x0);
    else if (strcmp(cmd, "gfx") == 0)               cmd_gfx();

    // --- Inconnu ---
    else {
        term->print("Unknown command: ");
        term->println(cmd);
        term->println("Type HELP for a list of commands.");
    }
}

extern "C" void kernel_main(unsigned long long mb_addr) {

    init(mb_addr);
    term->clear();
    term->set_color(GREEN);
    term->println("DOS64 v0.1");
    term->set_color(WHITE);
    char prompt[32];
    pm->get_prompt(prompt);
    term->print(prompt);


    // Buffer de saisie
    static char input[256];
    int input_len = 0;

    while (power) {
        cursor->update(mouse);

        // Lire le clavier (non bloquant)
        char c = kbd->poll();  // ← utilise poll() au lieu de getchar() !
        if (!c) continue;
        
        if (c == '\n') {
            // Terminer la chaîne
            input[input_len] = '\0';
            term->putchar('\n');

            // Interpréter seulement si non vide
            if (input_len > 0)
                interpret_command(input);

            // Réinitialiser le buffer
            input_len = 0;
            char prompt[32];
            pm->get_prompt(prompt);
            term->print(prompt);

        } else if (c == '\b') {
            // Backspace : effacer le dernier caractère
            if (input_len > 0) {
                input_len--;
                term->putchar('\b');
            }
            term->update_cursor();

        } else {
            // Caractère normal : ajouter au buffer si pas plein
            if (input_len < 255) {
                input[input_len++] = c;
                term->putchar(c);
            }
            term->update_cursor();
        }
    }
}
