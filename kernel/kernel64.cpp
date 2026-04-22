#include "drivers/keyboard.h"
#include "prompt.h"
#include "standart.h"
#include "memory.h"
#include "drivers/ata.h"
#include "panic.h" // ça aidera pour le débug et les erreurs critiques, même si pour l'instant on s'en sert pas beaucoup
#include "drivers/video.h"
#include "drivers/mouse.h" // pour les tests de souris, même remarque que pour FAT32.h
#include "fs/FAT32.h" // pour les tests de lecture de disque, on peut enlever ça plus tard quand le système de fichiers sera prêt, mais pour l'instant ça nous permet de vérifier que la lecture du disque fonctionne correctement en essayant de lire le boot sector et d'afficher le nom du volume
#include "elf64.h" // pour les tests de chargement d'ELF, même remarque que pour FAT32.h
#include "idt.h" // pour les tests d'interruptions, même remarque que pour FAT32.h
#include "drivers/beep.h" // pour les tests de son, même remarque que pour FAT32.h
#include "drivers/acpi.h" // pour les tests d'ACPI, même remarque que pour FAT32.h
#include "vmouse.h"
#include "fs/vfs.h"
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
bool power = false; // pour le moment on peut pas éteindre la machine, par manque de prise en charge de l'ACPI, mais ça peut servir pour un reboot ou une mise en veille plus tard

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

void init(unsigned long long mb_addr) {
    power = true;
    print("Initializing DOS64...", 0, t_color(LIGHT_CYAN));
    delay(10000000);

    print("Starting Terminal...", 1, t_color(LIGHT_GREEN));
    term = new (term_buf) Terminal();

    print("Initializing Keyboard...", 2, t_color(LIGHT_GREEN));
    kbd = new (kbd_buf) Keyboard();

    print("Initializing Memory...", 3, t_color(LIGHT_GREEN));
    mm = new (mm_buf) MemorySystem();
    MultibootInfo* mb = (MultibootInfo*)mb_addr;
    mm->init(mb, (unsigned long long)_kernel_end);  // passe kernel_end
    print("Memory OK", 4, t_color(LIGHT_GREEN));

    print("Initializing Heap...", 4, t_color(LIGHT_GREEN));
    heap = new (heap_buf) HeapAllocator();
    // Init de la Vidéo
    // ah bah non en fait
    
    // Heap juste après le kernel, alignée sur 4Ko
    unsigned long long heap_start = ((unsigned long long)_kernel_end + 4095) & ~4095;
    heap->init(heap_start, 4 * 1024 * 1024);
    // PAS de malloc de test — inutile et dangereux
    print("Heap OK", 5, t_color(LIGHT_GREEN));

    print("Initializing IDT...", 6, t_color(LIGHT_GREEN));
    idt    = new (idt_buf)  IDT;
    idt->init();

    print("Initializing ELF64...", 7, t_color(LIGHT_GREEN));
    elf_64 = new (elf_buf) ELF64Loader(heap);
    // on init le disque maintenant.
    //print("Initializing ATA...", 6, t_color(LIGHT_GREEN));
    // tant pis on va laisser l'utilisateur monter le disque lui même avec la commande MOUNT, comme ça on peut afficher un message d'erreur plus clair en cas de problème, et on évite de planter le kernel au démarrage juste parce que le disque est pas là ou mal branché
    //ata = new (ata_buf) ATADriver();
    //if (!ata) kernel_panic("ATADriver alloc failed", 0x001);
    //print("Initializing Video...", 6, t_color(LIGHT_GREEN));
    //init_video_mode();
    mount_ata(); // on peut pas faire mieux que ça pour init le disque, vu que le driver ATA est assez lourd et que ça fait beaucoup de choses à la fois, donc autant le faire dans une fonction séparée, et on pourra aussi l'appeler plus tard depuis l'interpréteur de commandes pour permettre à l'utilisateur de monter le disque lui même quand il veut, et d'afficher un message d'erreur plus clair en cas de problème, au lieu de planter le kernel au démarrage juste parce que le disque est pas là ou mal branché
    //bool disk_ok = ata->init();
    //if (!disk_ok) {
    //term->println("No disk found.");  // pas fatal, on continue
    //} else {
    //    term->print("Disk: ");
    //    term->println(ata->get_model());
    //}
    beep = new (beep_buf) Beeper;
    beep->beep_boot();  // bip de démarrage !

    acpi = new (acpi_buf) ACPI;
    acpi->init();

    mouse = new (mouse_buf) PS2Mouse;
    mouse->init();
    cursor = new (cursor_buf) MouseCursor;
    pm = new (path_buf) PathManager;
    pm->init();
    delay(9000000);
    print("Boot complete!", 8, t_color(LIGHT_GREEN, GREEN));
    delay(9000000);
    delay(9000000);
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

    // système de fichiers
    fs = new (fat_buf) FAT32;
    if (fs->mount(ata)) {
        term->println("FAT32 mounted on C:");
    } else {
        term->println("FAT32 mount failed - disk may not be formatted.");
    }

}
// --- INTERPRETEUR DE COMMANDES ---
// ============================================================
// INTERPRÉTEUR DE COMMANDES — DOS64
// ============================================================
// Helpers internes
// ============================================================

// Vérifie si un fichier/dossier existe et retourne ses attributs
// Retourne -1 si introuvable, sinon les attributs FAT32
static int fat_get_attr(const char* path) {
    if (!fs || !fs->is_mounted()) return -1;
    return fs->get_attributes(path);  // à implémenter dans fat32.h
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
    term->println("  MOUNT             Initialize ATA driver and mount C:");
    term->println("  DIR [path]        List files in current or given directory");
    term->println("  CD [path]         Change current directory (CD .. to go up)");
    term->println("  TYPE [file]       Display contents of a text file");
    term->println("  MKDIR [name]      Create a new directory");
    term->println("  RUN [file.elf]    Load and execute a 64-bit ELF binary");

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
        if (fs->mount(ata)) {
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
    if (!require_fs()) return;

    char resolved[256];
    if (arg && arg[0]) {
        pm->resolve(arg, resolved);
    } else {
        // Lister le répertoire courant
        pm->resolve("", resolved);
        // Si CWD est vide, lister la racine
        if (!resolved[0]) resolved[0] = '\0';
    }

    term->set_color(LIGHT_CYAN);
    term->print("Directory of ");
    char prompt[32];
    pm->get_prompt(prompt);
    // Afficher juste le chemin sans "> "
    for (int i = 0; prompt[i] && prompt[i] != '>'; i++)
        term->putchar(prompt[i]);
    term->putchar('\n');
    term->set_color(WHITE);

    fs->list_files(term, resolved);
}

static void cmd_cd(const char* path) {
    if (!require_fs()) return;

    if (!path || !path[0]) {
        term->print(pm->get_volume());
        term->print(":/");
        term->println(pm->get_cwd());
        return;
    }

    // .. et / toujours valides sans vérification
    if (strcmp(path, "..") == 0 || strcmp(path, "/") == 0 ||
        strcmp(path, "\\") == 0) {
        pm->cd(path);
        return;
    }

    // Vérifier que la cible est bien un dossier
    char resolved[256];
    pm->resolve(path, resolved);

    if (!fs->is_directory(resolved)) {
        term->print("Not a directory: ");
        term->println(path);
        return;
    }

    pm->cd(path);
}

static void cmd_type(const char* path) {
    if (!require_fs()) return;
    if (!require_arg(path, 0)) return;

    char resolved[256];
    pm->resolve(path, resolved);

    FAT32_File f = fs->open(resolved);
    if (!f.valid) {
        term->print("File not found: ");
        term->println(path);
        return;
    }

    static unsigned char file_buf[4096];
    unsigned int read_size = f.file_size < 4095 ? f.file_size : 4095;

    if (fs->read_file(&f, file_buf, read_size)) {
        file_buf[read_size] = '\0';
        term->println((char*)file_buf);
    } else {
        term->println("Error reading file.");
    }
}

static void cmd_mkdir(const char* name) {
    if (!require_fs()) return;
    if (!require_arg(name, 0)) return;

    char resolved[256];
    pm->resolve(name, resolved);

    if (fs->mkdir(resolved)) {
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

    FAT32_File f = fs->open(resolved);
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

    fs->read_file(&f, buf, f.file_size);
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

    //recherche d'abord un programmes dans le repertoire selectionné, s'il est présent,  on l'exécute et on ignore le reste.
    // Recherche d'un exécutable dans le répertoire courant
    // Essaie d'abord le nom exact, puis avec .ELF ajouté
    char resolved[256];
    char cmd_with_ext[260];

    // Essai 1 : nom exact (hello.elf tapé en entier)
    pm->resolve(cmd, resolved);
    bool found = (fat_get_attr(resolved) >= 0);

    // Essai 2 : ajouter .ELF automatiquement (hello → hello.elf)
    if (!found) {
        // Construire "cmd.ELF"
        int i = 0;
        for (; cmd[i]; i++) cmd_with_ext[i] = cmd[i];
        cmd_with_ext[i++] = '.';
        cmd_with_ext[i++] = 'E';
        cmd_with_ext[i++] = 'L';
        cmd_with_ext[i++] = 'F';
        cmd_with_ext[i]   = '\0';

        pm->resolve(cmd_with_ext, resolved);
        found = (fat_get_attr(resolved) >= 0);
    }

    if (found) { // execution du programme trouvé, avec ou sans extension .ELF
        cmd_run(cmd_with_ext[0] ? cmd_with_ext : cmd);
        return;
    }

    // --- Système ---
    if      (strcmp(cmd, "help") == 0)              cmd_help();
    else if (strcmp(cmd, "clear") == 0)             cmd_clear();
    else if (strcmp(cmd, "mem") == 0)               cmd_mem();
    else if (strncmp(cmd, "echo ", 5) == 0)         cmd_echo(cmd + 5);
    else if (strcmp(cmd, "shutup") == 0)            cmd_shutdown(false);
    else if (strncmp(cmd, "shutup r", 8) == 0)      cmd_shutdown(true);
    else if (strcmp(cmd, "reboot") == 0)            cmd_shutdown(true);

    // --- Filesystem ---
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
