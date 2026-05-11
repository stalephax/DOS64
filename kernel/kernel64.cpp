#include "drivers/keyboard.h"
#include "prompt.h"
#include "standart.h"
#include "memory.h"
#include "panic.h" // ça aidera pour le débug et les erreurs critiques, même si pour l'instant on s'en sert pas beaucoup
#include "drivers/video.h"
#include "drivers/mouse.h" // pour les tests de souris, même remarque que pour FAT32.h
#include "elf64.h" // pour les tests de chargement d'ELF, même remarque que pour FAT32.h
#include "idt.h" // pour les tests d'interruptions, même remarque que pour FAT32.h
#include "mzexe.h"
#include "drivers/beep.h" // pour les tests de son, même remarque que pour FAT32.h
#include "drivers/acpi.h" // pour les tests d'ACPI, même remarque que pour FAT32.h
#include "pointer.h"
#include "fs/vfs.h"
#include "fs/vdrive.h"      // 1. interface abstraite
#include "drivers/ata.h"    // 2. driver ATA
#include "drivers/ramdrv.h" // 3. driver RAM
#include "drivers/ata_lf.h" // 4. adapteur ATA
#include "drivers/ram_lf.h" // 5. adapteur RAM
#include "fs/FAT32.h"       // 6. filesystem (utilise DiskDrive*)
#include "apicore/kapi.h"   // pour l'interface d'API kernel passée aux drivers
#include "apicore/devtable.h"
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

  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@              
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%@@@@@%@@@@@%@@@@@@@%#####################%%@@@@@@        
  @@@@%########################%%%@@@@@%##*#%@%#*#%%%#*#%@@@@@*-:::::::::::::::::::-*%@@@@@@        
  @@@@%#######################%%@@@@@%%################%%@@#*+=:...................:=+****#%@@@     
  @@@@%#####################%%@@@@@@%#**#%%##*#%%%#*#%@@@@%+-............................:+#@@@     
  @@@@%#####################%@@@@%%####%#####%#####%%@@%*+=-:............................:--=*#@@@  
  @@@@%####################%%@@@%%####%%%###%@%###%%@@@%+:......::-=============---:::...:::-+#@@@  
  @@@@%#########%%%%%%%%%%%%@@@%###%%%%%%@@@@@@@@@%%%@@%+:......:=*%%%%%%%%%%%%%#*+=-:...:-=+#%@@@  
  @@@@%#########%%%%%%%%%@@@%%%###%%%%%%%%%%%%@@@@@%@@@%+:...:-=*#%@@%%%@@@@@@@@@%##+=---==+*#%@@@  
  @@@@%#########%%%%%%%%@@@@#**#%%%%%%@@%#####%@@@@@@@@%+:...:+%@@@@%#*#%@@@@@@ @@@@#*++++++*#%@@@  
  @@@@%########%%@@@@@@@@@@@%#####%%@@@@%#####%@@@@@@@@%+:...:+%@@@%%%####%@@@@ @@@@%%%%%%%%%%@@@@  
  @@@@%########%@@@@@@@@@@@@@%%###%%@@@@%#####%@@@@@@@@%+-::::+%@@%%%%%##*#%@@@@@@@@@@@@@@@@@@@     
  @@@@%########%@@@@@@@@@@@@%###%%%%%@@@%#####%@@@@@@@@%#+=-::+%@@@@%#*##%%@@@@@@@@@@@@@@@@@@@@     
  @@@@%########%@@@@    @@@@%###%%%%%@@@%#####%@@@@ @@@@%#*+==*%@@@%%####%%@@@%#####%%@@@@@@        
  @@@@%########%@@@@    @@@@@%%#*##%@@@@%#####%@@@@  @@@@@%#**#@@@%%%%%##*#%@%*-:::=*%@@@@@@        
  @@@@%########%@@@@    @@@@%#####%%%@@@%#####%@@@@    @@@@%%%%@@@@%%%####%@@%+:...:=+****#%@@@     
  @@@@%########%@@@@    @@@@%*##%%%%%@@@%#####%@@@@       @@@@@@@@@@%#*#%%@@@%+::::......:+#@@@     
  @@@@%########%@@@@    @@@@%%####%%@@@@%#####%@@@@       @@@@@@@@%%%%%###%@@%*=--:......:-=+*#@@@  
  @@@@%########%@@@@    @@@@@%%###%@@@@@%#####%@@@@     @@@@@@@@@@%%%%%##*#%@@%#*=-:::......:+#@@@  
  @@@@%########%@@@@    @@@@%###%%@@@@@@%#####%@@@@    @@@@@@@@@@@@@%#*##%%@@@@@%#+=-:......:+#@@@  
  @@@@%########%@@@@    @@@@%####%@@@@@%%#####%@@@@ @@@%#########%@@%%#%%@@@@@@@@%#*=-......:+#@@@  
  @@@@%########%@@@@       @@@@#*#%@@%######%%@@@@@ @@@%+:......:+%@@@@@@@@@    @@@%*-......:+#@@@  
  @@@@%########%@@@@@@@@@@@@@@@@%%%%%#####%%%@@@@@@@@@@%*=--:...:-=+#%@@@@@@@@@@%*+=-:......:+#@@@  
  @@@@%########%@@@@@@@@@@@@@@@@@@%######%%@@@@@@@@@@@@%#*+=:......:=*@@@@@@@@@@#+:......::::+#@@@  
  @@@@%##################################%%@@@@@@%###%@%#*+=:......::-==========-::......:-=+*%@@@  
  @@@@%################################%%%@@@@@%%####%@%#*+=--::......................::--=+*#%@@@  
  @@@@%##############################%@@@@@@@@%#*##%@@@%#*++++-:......................:-=+++*#%@@@  
  @@@@%#############################%%@@@@@%%#######%%@@%##**++=-----:.............::-==++++*#%@@@  
  @@@@%###########################%%%@@@@@%%#*#%%%#*##%@@@@%****++++=-::::::::::::::=++++****#%@@@  
  @@@@%####%%%%%%%%%%%%%%%%%%%%%%%%%%@@@@%%%%%%%%%%%%%%%@@@@%%#*+++++===============+++++*##%%@@@@  
  @@@@%##%%%%%%%%%%%%%%%%%%%%%%%%%%%@@@@@%%%%@%%%%%@@%%%%@@@@@%#***++++++++++++++++******#%@@@@     
  @@@@@%%%%%%%%%%%%%%%%%%%%%%%%%%%@@@@@@@@@%%%%%@%%%%%@@%%%%@@@@%#*++++++++++++++++*#%@@@@@@@@      
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@%@@@@@%@@@@@%@@@@@@@%##################%@@@@@@@        
     @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@              
     @@@@@@@@@@@@@@@@@@@@@@@%#************##%@@@@@@@@@@@@@@@@@@@@%#****%@@@@@@@@@@@@@@              
                      @@@@@%*==============*#@@@@@          @@@@%#+====*%@@@                        
                     @@@%#*++==============+++#%@@@       @@@@%**+=====*%@@@                        
                     @@@%*=====+********+=====*%@@@       @@@@#+=======*%@@@                        
                     @@@%*====+#%@@@@@@%#+====*%@@@       @@@@#+=======*%@@@                        
                 @@@@%%#*+=++*#%@@@@@@@@%#****%@@@@    @@@@%##*+=======*%@@@                        
                 @@@@#*====+#@@@@@@@@@@@@@@@@@@@@@@    @@@%#+==========*%@@@                        
                 @@@@#*====+#@@%********%@@@@@@@@@@ @@@%#**+==*##*+====*%@@@                        
                 @@@@#+====+#%#*+======+*#@@@@@     @@@%*+===+*%@#+====*%@@@                        
                 @@@@#+=====++++========++*#%@@@    @@@%*+===+*%@#+====*%@@@                        
                 @@@@#+=======++*****+=====+*#%%@@@@@%##*==+**%@@#+====*%@@@                        
                 @@@@#+=======+#@@@@@#+=======*%@@@@%#+===+#%@@@@#+====*%@@@                        
                 @@@@#+====++*#%@@@@@%#**+====*%@@%#*+=+**#%@@@@@#+====*%@@@                        
                  @@@#*====+#@@@@   @@@@#+====*%@%*+===+#%@@@@@@%#+====*#@@@@@                      
                 @@@@%#*++=+#@@@@   @@@@#+=++*#%@%#*++==+********+=====++*#%@@@                     
                 @@@@@%%#*+*#@@@@@@@@@@%#*+**#%@@@%#**+++++++++++=====++++*%@@@                     
                   @@@@@%%####%%@@@@@%%#######%@@@%####**********++=+*****#%@@@                     
                     @@@@%%%###%%%%%%%######%%%@@@%%%%%%%%%%%%%%%#*+**##%%%@@@@                     
                        @@@@%##############%@@@@@@@@@@@@@@@@@@@@@%%####%@@@@                        
                        @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@                        
                        @@@@@@@@@@@@@@@@@@@@@@@@              @@@@@@@@@@@@@@@                        
*/








inline void* operator new(unsigned long, void* p) { return p; }
// Adresse mémoire du buffer VGA (texte mode)
static unsigned short* const T_VGA = (unsigned short*)0xB8000;
// périphériques globaux (instanciés dans init() avec placement new)
static unsigned char term_buf[sizeof(Terminal)];
static unsigned char kbd_buf[sizeof(Keyboard)];
static unsigned char mm_buf[sizeof(MemorySystem)];
static unsigned char heap_buf[sizeof(HeapAllocator)];
static unsigned char ata_buf[sizeof(ATADriver)];
static unsigned char fat_buf[sizeof(FAT32)];
static unsigned char elf_buf[sizeof(ELF64Loader)];
static unsigned char mz_buf[sizeof(MZExeLoader)];
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
MZExeLoader* mz_exe;
VGAGraphics* vga; // pour les tests de vidéo
Beeper* beep; // pour les tests de son
PS2Mouse* mouse; // pour les tests de souris
ACPI* acpi; // pour les tests d'ACPI, même remarque que pour FAT32.h
RamDisk* ramdisk;
FAT32*   ramfs;
ATADiskIF* ata_if;
RamDiskIF* ram_if;
bool power = false; // pour le moment on peut pas éteindre la machine, par manque de prise en charge de l'ACPI, mais ça peut servir pour un reboot ou une mise en veille plus tard
// périphériques instanciés des pilotes .SYS
static unsigned char devtable_buf[sizeof(DeviceTable)];
DeviceTable* devtable;

// Fonctions wrapper pour la KernelAPI
static int mz_host_bios_int(unsigned char intn, RealModeRegs* r, void* ctx) {
    if (intn == 0x10) { // Interruption Vidéo BIOS
        unsigned char ah = (unsigned char)(r->ax >> 8);
        unsigned char al = (unsigned char)(r->ax & 0xFF);

        // AH=00h : Set Video Mode
        if (ah == 0x00) {
            if (al == 0x13) { // Mode 320x200 256c
                vga = ensure_vga();
                vga->init(); // Active le mode 13h via les ports I/O
                return 0;
            }
            if (al == 0x03) { // Retour au mode Texte
                vga = ensure_vga();
                vga->text_mode();
                return 0;
            }
        }
    }
    (void)ctx;
    if (!r) return -1;

    if (intn == 0x10) {
        unsigned char ah = (unsigned char)(r->ax >> 8);
        unsigned char al = (unsigned char)(r->ax & 0xFF);

        // BIOS video set mode
        if (ah == 0x00 && al == 0x13) {
            if (!vga) vga = new (vga_buf) VGAGraphics;
            vga->init();
            vga->clear(VGAGraphics::BLACK);
            return 0;
        }

        // teletype output fallback
        if (ah == 0x0E) {
            if (term) term->putchar((char)al);
            return 0;
        }
        return 0;
    }

    // keyboard/time interrupts can be stubbed as successful for now.
    if (intn == 0x16 || intn == 0x1A) return 0;

    return -1;
}

static void mz_host_port_out(unsigned short port, unsigned char val, void* ctx) {
    (void)ctx;
    outb(port, val); 
}

static unsigned char mz_host_port_in(unsigned short port, void* ctx) {
    (void)ctx;
    return inb(port);
}

static void mz_host_putc(char c, void* ctx) {
    (void)ctx;
    if (term) term->putchar(c);
}

static void kapi_print(const char* s)               { term->print(s); }
static void kapi_println(const char* s)             { term->println(s); }
static void kapi_set_color(unsigned char fg, unsigned char bg) { term->set_color(fg, bg); }
static void* kapi_malloc(unsigned long long sz)     { return heap->malloc(sz); }
static void kapi_free(void* p)                      { heap->free(p); }
static unsigned char kapi_inb(unsigned short p)     { return inb(p); }
static void kapi_outb(unsigned short p, unsigned char v) { outb(p, v); }
static unsigned short kapi_inw(unsigned short p)    { return inw(p); }
static void kapi_outw(unsigned short p, unsigned short v) { outw(p, v); }
static void kapi_register_device(const char* name, void* ptr, int type) {
    if (devtable) devtable->register_device(name, ptr, type);
}

// Instance globale de l'API — partagée avec tous les drivers
static KernelAPI g_kapi = {
    kapi_print,
    kapi_println,
    kapi_set_color,
    kapi_malloc,
    kapi_free,
    kapi_inb,
    kapi_outb,
    kapi_inw,
    kapi_outw,
    kapi_register_device
};
struct LoadedDriverInfo {
    char name[64];
    int last_exit_code;
    bool used;
};

static LoadedDriverInfo g_loaded_drivers[16];
static int g_loaded_driver_count = 0;

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

void delay (/* ha ah, essaye de m'attrapper compileur de merde !*/ const long long d) // delai décimal simple
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
    delay(6543458U);
    // --------------------------------------------------------
    // PHASE 1 : Terminal (requis pour afficher quoi que ce soit après)
    // --------------------------------------------------------
    term = new (term_buf) Terminal;
    init_status("Terminal                  [ OK ]", 2, true);

    // --------------------------------------------------------
    // PHASE 2 : Clavier
    // --------------------------------------------------------
    kbd = new (kbd_buf) Keyboard;
    init_status("Keyboard                  [ OK ]", 3, true);

    // --------------------------------------------------------
    // PHASE 3 : Mémoire physique
    // --------------------------------------------------------
    if (mb_addr == 0 || mb_addr > 0x3FFFFFFF) {
        init_status("Memory                [FAIL] bad mb_addr", 4, false);
        asm volatile("cli; hlt");
    }
    mm = new (mm_buf) MemorySystem;
    MultibootInfo* mb = (MultibootInfo*)mb_addr;

    // Vérifier le flag mmap (bit 6)
    if (!(mb->flags & (1 << 6))) {
        init_status("Memory                [FAIL] no mmap", 4, false);
        asm volatile("cli; hlt");
    }
    mm->init(mb, (unsigned long long)_kernel_end);
    init_status("Memory                    [ OK ]", 4, true);
    init_debug(mm->get_free_mb(), 4, 50);

    // --------------------------------------------------------
    // PHASE 4 : Heap
    // --------------------------------------------------------
    heap = new (heap_buf) HeapAllocator;
    unsigned long long heap_start = ((unsigned long long)_kernel_end + 4095) & ~4095;
    heap->init(heap_start, 4 * 1024 * 1024);
    init_status("Heap                      [ OK ]", 5, true);
    init_debug(heap_start, 5, 50);

    // --------------------------------------------------------
    // PHASE 5 : IDT + interruptions
    // --------------------------------------------------------
    idt = new (idt_buf) IDT;
    idt->init();
    init_status("IDT                       [ OK ]", 6, true);

    // --------------------------------------------------------
    // PHASE 6 : ELF64 Loader
    // --------------------------------------------------------
    elf_64 = new (elf_buf) ELF64Loader(heap);
    mz_exe = new (mz_buf) MZExeLoader(heap);
    mz_exe->bind_host_console(mz_host_putc, nullptr);
    mz_exe->bind_host_bios(mz_host_bios_int, nullptr);
    mz_exe->bind_host_ports(mz_host_port_in, mz_host_port_out, nullptr);
    init_status("ELF64/MS-DOS Executive    [ OK ]", 7, true);

    // --------------------------------------------------------
    // PHASE 7 : ACPI (requis pour shutdown/reboot)
    // --------------------------------------------------------
    acpi = new (acpi_buf) ACPI;
    acpi->init();
    init_status("ACPI                      [ OK ]", 8, true);

    // --------------------------------------------------------
    // PHASE 8 : Souris + curseur
    // --------------------------------------------------------
    mouse = new (mouse_buf) PS2Mouse;
    bool mouse_ok = mouse->init();
    init_status("PS2 Mouse                 [ OK ]", 9, mouse_ok);
    cursor = new (cursor_buf) MouseCursor;

    // --------------------------------------------------------
    // PHASE 9 : Path Manager (VFS minimal)
    // --------------------------------------------------------
    pm = new (path_buf) PathManager;
    pm->init();
    init_status("Path Manager              [ OK ]", 10, true);

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


    // NOUVEAU : On branche les ports I/O
    

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
        fuckup("ATA Driver alloc failed", 0x001);
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

static bool has_extension(const char* path) {
    int dot = -1;
    for (int i = 0; path[i]; i++) {
        if (path[i] == '/' || path[i] == '\\') dot = -1;
        else if (path[i] == '.') dot = i;
    }
    return dot >= 0;
}

static bool append_ext(const char* base, const char* ext, char* out, int out_cap) {
    int i = 0;
    for (; base[i] && i < out_cap - 1; i++) out[i] = base[i];
    if (base[i] != '\0') return false;
    int j = 0;
    while (ext[j] && i < out_cap - 1) out[i++] = ext[j++];
    if (ext[j] != '\0') return false;
    out[i] = '\0';
    return true;
}



static bool resolve_runnable_path(const char* in_path, bool prefer_sys, char* out_path, int out_cap) {
    char resolved[256];
    FAT32* cur = current_fs();
    if (!cur) return false;

    // 1) Chemin fourni tel quel
    pm->resolve(in_path, resolved);
    FAT32_File f = cur->open(resolved);
    if (f.valid) {
        int i = 0;
        for (; in_path[i] && i < out_cap - 1; i++) out_path[i] = in_path[i];
        out_path[i] = '\0';
        return in_path[i] == '\0';
    }

    if (has_extension(in_path)) return false;

    // 2) Extension préférée
    if (append_ext(in_path, prefer_sys ? ".SYS" : ".ELF", out_path, out_cap)) {
        pm->resolve(out_path, resolved);
        f = cur->open(resolved);
        if (f.valid) return true;
    }


    if (!prefer_sys && append_ext(in_path, ".EXE", out_path, out_cap)) {
        pm->resolve(out_path, resolved);
        f = cur->open(resolved);
        if (f.valid) return true;
    }
    // 3) Fallback inverse
    if (append_ext(in_path, prefer_sys ? ".ELF" : ".SYS", out_path, out_cap)) {
        pm->resolve(out_path, resolved);
        f = cur->open(resolved);
        if (f.valid) return true;
    }
    return false;
}

 

// la méthode d'avant est éclatée au sous-sol, celle la sera mieux pas de crash
static int run_resolved_path(const char* path, bool is_driver = false) {
    if (!path || !path[0]) return -100;
    
    char resolved[256];
    pm->resolve(path, resolved);

    FAT32* cur = current_fs();
    if (!cur || !cur->is_mounted()) return -102;

    FAT32_File f = cur->open(resolved);
    if (!f.valid) return -100;
    if (f.file_size == 0) return -103;

    // Debug — afficher la taille du fichier
    char dbg[32];
    ulltoa(f.file_size, dbg);
    term->print("File size: "); term->println(dbg);

    unsigned char* buf = (unsigned char*)heap->malloc(f.file_size);
    if (!buf) return -101;

    // Debug — afficher l'adresse du buffer
    ulltoa((unsigned long long)buf, dbg);
    term->print("Buffer at: 0x"); term->println(dbg);

    cur->read_file(&f, buf, f.file_size);
    term->println("File read OK");

    // Debug — vérifier le magic ELF
    if (buf[0]==0x7F && buf[1]=='E' && buf[2]=='L' && buf[3]=='F')
        term->println("ELF magic OK");
    else
        term->println("NOT an ELF!");
    int code;
    if (is_driver) {
        term->println("Calling load_and_call_driver...");
        code = elf_64->load_and_call_driver(buf, f.file_size, &g_kapi);
    } else if (mz_exe && mz_exe->is_mz_exe(buf, f.file_size)) {
        term->println("MZ executable detected.");
        RealModeRegs regs;
        DOSPSP* psp = nullptr;
        unsigned char* rm_mem = nullptr;
        unsigned short load_seg = 0;
        code = mz_exe->build_real_mode_image(buf, f.file_size, &rm_mem, &regs, &psp, &load_seg);
        if (code == 0) {
            term->println("16-bit image loaded + relocations applied.");
            int dos_exit = 0;
            code = mz_exe->execute_real_mode_stub(rm_mem, &regs, 50000000000000, &dos_exit); // le budget des cycles doit augmenter
            if (code == 0) {
                current_program.running = false;
                current_program.exit_code = dos_exit;
                code = dos_exit;
                
            } else if (code == -30) {
                RMTraceState tr = mz_exe->get_last_trace();
                char hex[32];
                term->println("Infinite loop detected!");
                term->print("  CS="); ulltoa(tr.cs, hex); term->println(hex);
                term->print("  IP="); ulltoa(tr.ip, hex); term->println(hex);
                term->print("  OPCODE=0x"); ulltoa(tr.opcode, hex); term->println(hex);
                term->println("Word is waiting for something (keyboard? BIOS int?)");
            }
             else if (code == -10 || code == -20 || code == -21) {
                RMTraceState tr = mz_exe->get_last_trace();
                char hex[32];
                term->println("ERR: Real-mode fault trace:");
                term->print("  reason=");
                ulltoa((unsigned long long)code, hex); term->println(hex);
                term->print("  CS="); ulltoa((unsigned long long)tr.cs, hex); term->println(hex);
                term->print("  IP="); ulltoa((unsigned long long)tr.ip, hex); term->println(hex);
                term->print("  OPCODE="); ulltoa((unsigned long long)tr.opcode, hex); term->println(hex);
                term->print("  MODRM/INT="); ulltoa((unsigned long long)tr.modrm, hex); term->println(hex);
                term->println("Use this opcode to extend the 80186 emulator.");
            } else if (code == -8) {
                term->println("Real-mode execution timed out (step budget exceeded).");
            }
        }
        if (psp) heap->free(psp);
        if (rm_mem) heap->free(rm_mem);
    } else {
        code = elf_64->load_and_run(buf, f.file_size);
    }
    
    heap->free(buf);
    return code;
}

static void register_loaded_driver(const char* path, int exit_code) {
    if (!path || !path[0]) return;

    for (int i = 0; i < 16; i++) {
        if (!g_loaded_drivers[i].used) continue;
        if (strcmp(g_loaded_drivers[i].name, path) == 0) {
            g_loaded_drivers[i].last_exit_code = exit_code;
            return;
        }
    }

    for (int i = 0; i < 16; i++) {
        if (g_loaded_drivers[i].used) continue;

        int j = 0;
        for (; path[j] && j < 63; j++) g_loaded_drivers[i].name[j] = path[j];
        g_loaded_drivers[i].name[j] = '\0';
        g_loaded_drivers[i].last_exit_code = exit_code;
        g_loaded_drivers[i].used = true;
        g_loaded_driver_count++;
        return;
    }
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
    term->set_color(WHITE); // bruh missing half of the important commands.
    term->println("  FATFS             Format a volume as FAT32 (Trusted Intaller mode required)");
    term->println("  VOLS              List available volumes");
    term->println("  MOUNT             Initialize ATA driver and mount C:");
    term->println("  DIR [path]        List files in current or given directory");
    term->println("  CD [path]         Change current directory (CD .. to go up)");
    term->println("  TYPE [file]       Display contents of a text file");
    term->println("  MKDIR [name]      Create a new directory");
    term->println("  RMDIR [name]      Get rid of a directory");
    term->println("  RUN [file]        Load .ELF/.SYS/.EXE (EXE = 16-bit real-mode WIP)");
    term->println("  DVCMGR            List loaded drivers");
    term->println("  DVCMGR i [file]   Load a driver (.SYS/.ELF) now");
    //term->println("  [file(.elf/.sys)] Execute program directly by typing its name"); useless, the first thing everyone has an idea of.

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

// commande Formattage fat32, pour les tests de formatage et d'écriture sur disque, même remarque que pour FAT32.h
static void cmd_fatfs(const char* args) {
    // Parser : FATFS [C:] [label]
    // Exemples :
    //   FATFS           → formate C: avec label "DOS64"
    //   FATFS C:        → formate C: avec label "DOS64"
    //   FATFS A:        → formate A: (RAM disk)
    //   FATFS C: MYVOL  → formate C: avec label "MYVOL"

    char vol   = 'C';
    const char* label = "DOS64      ";
    char label_buf[12] = "DOS64      ";

    // Parser le volume
    if (args && args[0]) {
        if (args[1] == ':') {
            vol = args[0];
            args += 2;
            while (args[0] == ' ') args++;  // skip espaces
        }
        // Parser le label
        if (args[0]) {
            int i = 0;
            for (; args[i] && i < 11; i++) {
                char c = args[i];
                if (c >= 'a' && c <= 'z') c -= 32;  // majuscule
                label_buf[i] = c;
            }
            for (; i < 11; i++) label_buf[i] = ' ';
            label_buf[11] = '\0';
            label = label_buf;
        }
    }

    // Confirmation de sécurité
    term->set_color(LIGHT_RED);
    term->print("WARNING: This will ERASE all data on ");
    term->putchar(vol); term->println(":");
    term->set_color(WHITE);
    term->print("Type YES to confirm: ");

    // Lire la confirmation
    static char confirm[8];
    int ci = 0;
    while (ci < 7) {
        char c =kbd->getchar();
        if (c == '\n') break;
        if (c == '\b' && ci > 0) { ci--; term->putchar('\b'); continue; }
        confirm[ci++] = c;
        term->putchar(c);
    }
    confirm[ci] = '\0';
    term->putchar('\n');

    if (strcmp(confirm, "YES") != 0) {
        term->println("Format cancelled.");
        return;
    }

    // Choisir le bon disque
    FAT32* target_fs = nullptr;
    if (vol == 'C' || vol == 'c') target_fs = fs;
    if (vol == 'A' || vol == 'a') target_fs = ramfs;

    if (!target_fs) {
        term->println("Volume not available.");
        return;
    }

    term->print("Formatting ");
    term->putchar(vol); term->print(": as FAT32");
    if (label_buf[0] != ' ') {
        term->print(" [");
        term->print(label_buf);
        term->print("]");
    }
    term->println("...");

    if (target_fs->format(label)) {
        term->set_color(LIGHT_GREEN);
        term->print("Format complete. ");
        term->putchar(vol); term->println(": is ready.");
        term->set_color(WHITE);
    } else {
        term->set_color(LIGHT_RED);
        term->println("Format failed.");
        term->print(" Operation failed miserably. ");
        term->set_color(WHITE);
    }
}
static void cmd_keymap(const char* arg) {
    if (!arg || !arg[0]) {
        term->print("Current layout: ");
        term->println(kbd->get_layout_name());
        term->println("Usage: KEYMAP <file.ktt>");
        return;
    }

    // Résoudre le chemin
    char resolved[256];
    pm->resolve(arg, resolved);

    FAT32* cur = current_fs();
    if (!cur) { term->println("No filesystem."); return; }

    FAT32_File f = cur->open(resolved);
    if (!f.valid) {
        term->print("Layout file not found: ");
        term->println(arg);
        return;
    }

    // Allouer et charger le fichier
    unsigned char* buf = (unsigned char*)heap->malloc(f.file_size);
    if (!buf) { term->println("Out of memory."); return; }

    cur->read_file(&f, buf, f.file_size);

    if (kbd->load_layout(buf, f.file_size)) {
        term->print("Layout loaded: ");
        term->println(kbd->get_layout_name());
        // Ne pas libérer buf — le layout reste en mémoire !
    } else {
        term->println("Invalid layout file.");
        heap->free(buf);
    }
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
// makes the system actually shut up
static void cmd_shutup(char bootmode) {
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
        beep->beep_error();
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
        term->println("ERR reading file.");
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

// son frère jumeau maléfique
static void cmd_rmdir(const char* name) {
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
        term->print_fail();
    }
}



static void cmd_run(const char* path) {
    if (!require_fs()) return;
    if (!require_arg(path, 0)) return;

    char runnable[260];
    if (!resolve_runnable_path(path, false, runnable, sizeof(runnable))) {
        term->print("File not found: ");
        term->println(path);
        return;
    }

    int code = run_resolved_path(runnable); // si l'opération échoue, code < 0, sinon c'est le code de retour du programme, et a indique son execution correcte

    switch (code) {
        case -100: term->println("ERR: file disappeared before execution."); break;
        case -101: term->println("ERR: Not enough memory to load program."); break;
        case -1:   term->println("ERR: not a valid ELF file."); break;
        case -2:   term->println("ERR: not an executable ELF."); break;
        case -3:   term->println("ERR: not x86_64 architecture."); break;
        case 67:   term->println("ERR: the software manufacturer found this joke funny. SIX-SEVEEEN ;)"); break;
        default: {
            char ret[16];
            ulltoa((unsigned long long)code, ret);
            term->print("Process exited with code: ");
            term->println(ret);
        }
    }
}

static void cmd_dvcmgr(const char* args) {
    if (!args || !args[0]) {
        term->set_color(LIGHT_CYAN);
        term->println("=== Device Manager ===");
        term->set_color(WHITE);

        if (g_loaded_driver_count == 0) {
            term->println("No drivers loaded.");
            return;
        }

        for (int i = 0; i < 16; i++) {
            if (!g_loaded_drivers[i].used) continue;
            term->print("  ");
            term->print(g_loaded_drivers[i].name);
            term->print("  (exit=");
            char code_buf[16];
            ulltoa((unsigned long long)g_loaded_drivers[i].last_exit_code, code_buf);
            term->print(code_buf);
            term->println(")");
        }
        return;
    }

    // DVCMGR i <pilote>
    if (!(args[0] == 'i' || args[0] == 'I') || args[1] != ' ' || !args[2]) {
        term->println("Usage:");
        term->println("  DVCMGR");
        term->println("  DVCMGR i <driver.sys>");
        return;
    }

    if (!require_fs()) return;

    const char* path = args + 2;
    char runnable[260];
    if (!resolve_runnable_path(path, true, runnable, sizeof(runnable))) {
        term->print("Driver not found: ");
        term->println(path);
        return;
    }

    term->print("Loading driver: ");
    term->println(runnable);
    int code = run_resolved_path(runnable, true);

    if (code >= 0) {
        register_loaded_driver(runnable, code);
        char ret[16];
        ulltoa((unsigned long long)code, ret);
        term->print("Driver started, exit code: ");
        term->println(ret);
        return;
    }

    switch (code) {
        case -100: term->println("ERR: driver file disappeared before execution."); break;
        case -101: term->println("Not enough memory to load driver."); break;
        case -1: term->println("ERR: not a valid ELF file."); break;
        case -2: term->println("ERR: not an executable ELF."); break;
        case -3: term->println("ERR: not x86_64 architecture."); break;
        default: term->println("Driver start failed."); break;
    }
    term->println("NOTICE : to know a driver installed correctly, the exit code must be 0 !");
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

static void cmd_xcopy(const char* args) {
    if (!require_fs());
    // parser les fichiers
    char o[256], d[256];
    int i = 0;
    while (args[i] && args[i] != ' ') { o[i] = args[i]; i++; }
    o[i] = '\0';
    if (!args[i]) { term->println("Usage: RENAME oldname newname"); return; }
    i++;
    int j = 0;
    while (args[i]) { d[j++] = args[i++]; }
    d[j] = '\0';

    char resolved[256];
    pm->resolve(o, resolved);

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
    // Si trouvé, on l'exécute directement (avec ou sans extension ELF/SYS).
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
        // Essai 3 : ajouter .SYS automatiquement (ex: driver -> driver.SYS)
        if (!found) {
            int i = 0;
            for (; token[i] && i < 255; i++) token_with_ext[i] = token[i];
            token_with_ext[i++] = '.';
            token_with_ext[i++] = 'S';
            token_with_ext[i++] = 'Y';
            token_with_ext[i++] = 'S';
            token_with_ext[i] = '\0';

            pm->resolve(token_with_ext, resolved);
            found = fat_file_exists(resolved);
        }

        // Essai 4 : ajouter .EXE automatiquement (ex: app -> app.EXE)
        if (!found) {
            int i = 0;
            for (; token[i] && i < 255; i++) token_with_ext[i] = token[i];
            token_with_ext[i++] = '.';
            token_with_ext[i++] = 'E';
            token_with_ext[i++] = 'X';
            token_with_ext[i++] = 'E';
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
    else if (strcmp(cmd, "keymap") == 0)            cmd_keymap(nullptr);
    else if (strncmp(cmd, "keymap ", 7) == 0)       cmd_keymap(cmd + 7);
    else if (strncmp(cmd, "echo ", 5) == 0)         cmd_echo(cmd + 5);
    else if (strcmp(cmd, "shutup") == 0)            cmd_shutup('s');
    else if (strncmp(cmd, "shutup -r", 8) == 0)      cmd_shutup('r');
    else if (strcmp(cmd, "reboot") == 0)            cmd_shutup('r');

    // --- Filesystem ---
    else if (strncmp(cmd, "fatfs", 5) == 0)         cmd_fatfs(cmd + 5);
    else if (strcmp(cmd, "vols") == 0)              cmd_vols();
    else if (strcmp(cmd, "mount") == 0)             cmd_mount();
    else if (strcmp(cmd, "dir") == 0)               cmd_dir(nullptr);
    else if (strncmp(cmd, "dir ", 4) == 0)          cmd_dir(cmd + 4);
    else if (strcmp(cmd, "cd") == 0)                cmd_cd(nullptr);
    else if (strncmp(cmd, "cd ", 3) == 0)           cmd_cd(cmd + 3);
    else if (strncmp(cmd, "type ", 5) == 0)         cmd_type(cmd + 5);
    else if (strncmp(cmd, "mkdir ", 6) == 0)        cmd_mkdir(cmd + 6);
    else if (strncmp(cmd, "run ", 4) == 0)          cmd_run(cmd + 4);
    else if (strcmp(cmd, "dvcmgr") == 0)            cmd_dvcmgr(nullptr);
    else if (strncmp(cmd, "dvcmgr ", 7) == 0)       cmd_dvcmgr(cmd + 7);
    // Dans le dispatch :
    else if (strncmp(cmd, "rename ", 7) == 0)       cmd_rename(cmd + 7);
    else if (strncmp(cmd, "del ", 4) == 0)          cmd_del(cmd + 4);
    else if (strncmp(cmd, "xcopy ", 5) == 0)     cmd_xcopy(cmd + 5); 
    // --- Debug ---
    else if (strcmp(cmd, "laxative") == 0)          cmd_laxative();
    else if (strncmp(cmd, "wraw ", 5) == 0)         cmd_wraw(cmd + 5);
    else if (strcmp(cmd, "fkup") == 0)              fuckup("DEBUG TESTING", 0x0);
    else if (strcmp(cmd, "gfx") == 0)               cmd_gfx();

    // --- Inconnu ---
    else {
        term->print("Unknown command: ");
        term->println(cmd);
        term->println("Type HELP for a list of commands.");
    }
}

/// @brief Point d'entrée du système
/// @param mb_addr adresse du BSS attribuée par l'amorçeur
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
    int i_writecurpos = 0; // position d'écriture
    int input_len = 0;

    // Historique de saisie : 256 caractères max

    static char old_input[128][256];
    int old_input_lengh;
    

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
            // mettre à jour l'historique
            old_input_lengh++;
            old_input[old_input_lengh][256] = *input;
            char prompt[32];
            pm->get_prompt(prompt);
            term->print(prompt);

        } else if (c == '\b') {
            // Backspace : effacer le dernier caractère
            if (input_len > 0) {
                input_len--;
                term->putchar('\b');
                i_writecurpos = input_len;
            }
            //term->update_cursor();
        else if (c == '\t') { 
            for (int c = 0; c < old_input_lengh; c++ ) {
                term->putchar(*old_input[c]);
            }
        }

        } else {
            // Caractère normal : ajouter au buffer si pas plein
            if (input_len < 255) {
                input[input_len++] = c;
                term->putchar(c);
                i_writecurpos = input_len;
            }
            //term->update_cursor();
        }
    }
}
// FIN du kernel