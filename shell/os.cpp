#include "lib/std.h"

// ============================================================
// DOS64 Graphical Shell v2.1 - Support Souris Simplifié
// ============================================================

static const int SCREEN_W = 320;
static const int SCREEN_H = 200;

// Palette de couleurs Windows 1.0
static const unsigned char COL_WINDOW_BG    = 15;
static const unsigned char COL_WINDOW_FRAME = 0;
static const unsigned char COL_TITLE_BAR   = 1;
static const unsigned char COL_TITLE_TEXT  = 15;
static const unsigned char COL_DESKTOP     = 3;
static const unsigned char COL_ICON_NORMAL = 7;
static const unsigned char COL_ICON_SEL    = 9;
static const unsigned char COL_TEXT_NORMAL = 0;
static const unsigned char COL_TEXT_SEL    = 15;
static const unsigned char COL_CURSOR      = 4;

// Dimensions
static const int WINDOW_W = 300;
static const int WINDOW_H = 160;
static const int WINDOW_X = (SCREEN_W - WINDOW_W) / 2;
static const int WINDOW_Y = (SCREEN_H - WINDOW_H) / 2;
static const int TITLE_H = 18;
static const int DOUBLE_CLICK_TIME = 20;

// Police bitmap (gardez votre police existante)
static const unsigned char font[128][5] = {
    // ... gardez toute votre police existante ici ...
    // 0-31: caractères de contrôle (vides)
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x00,0x00,0x00},
    
    // 32-63: espace et ponctuation + chiffres
    {0x00,0x00,0x00,0x00,0x00}, // 32 espace
    {0x44,0x44,0x44,0x00,0x04}, // 33 !
    {0x54,0x54,0x54,0x00,0x00}, // 34 "
    {0x54,0x54,0x7c,0x54,0x54}, // 35 #
    {0x50,0x78,0x50,0x50,0x50}, // 36 $
    {0x54,0x28,0x10,0x28,0x54}, // 37 %
    {0x48,0x54,0x54,0x54,0x24}, // 38 &
    {0x00,0x04,0x04,0x00,0x00}, // 39 '
    {0x08,0x10,0x20,0x40,0x20}, // 40 (
    {0x20,0x40,0x20,0x10,0x08}, // 41 )
    {0x50,0x50,0x7c,0x50,0x50}, // 42 *
    {0x10,0x10,0x7c,0x10,0x10}, // 43 +
    {0x00,0x00,0x00,0x04,0x04}, // 44 ,
    {0x10,0x10,0x10,0x10,0x10}, // 45 -
    {0x00,0x00,0x00,0x00,0x04}, // 46 .
    {0x04,0x08,0x10,0x20,0x40}, // 47 /
    {0x7c,0x44,0x44,0x44,0x7c}, // 48 0
    {0x04,0x04,0x04,0x04,0x04}, // 49 1
    {0x7c,0x04,0x7c,0x20,0x7c}, // 50 2
    {0x7c,0x04,0x1c,0x04,0x7c}, // 51 3
    {0x44,0x44,0x7c,0x04,0x04}, // 52 4
    {0x7c,0x40,0x7c,0x04,0x7c}, // 53 5
    {0x7c,0x40,0x7c,0x44,0x7c}, // 54 6
    {0x7c,0x04,0x08,0x10,0x10}, // 55 7
    {0x7c,0x44,0x7c,0x44,0x7c}, // 56 8
    {0x7c,0x44,0x7c,0x04,0x7c}, // 57 9
    
    // 64-95: lettres majuscules
    {0x08,0x10,0x20,0x40,0x00}, // 64 @
    {0x38,0x44,0x44,0x44,0x38}, // 65 A
    {0x7c,0x44,0x7c,0x44,0x7c}, // 66 B
    {0x38,0x44,0x40,0x44,0x38}, // 67 C
    {0x6c,0x54,0x54,0x54,0x6c}, // 68 D
    {0x7c,0x48,0x70,0x40,0x40}, // 69 E
    {0x38,0x40,0x70,0x40,0x38}, // 70 F
    {0x38,0x44,0x44,0x54,0x5c}, // 71 G
    {0x44,0x44,0x7c,0x44,0x44}, // 72 H
    {0x74,0x44,0x44,0x44,0x74}, // 73 I
    {0x0c,0x04,0x04,0x44,0x38}, // 74 J
    {0x44,0x4c,0x70,0x4c,0x44}, // 75 K
    {0x40,0x40,0x40,0x40,0x7c}, // 76 L
    {0x44,0x6c,0x54,0x6c,0x44}, // 77 M
    {0x54,0x64,0x44,0x44,0x44}, // 78 N
    {0x38,0x44,0x44,0x44,0x38}, // 79 O
    {0x6c,0x54,0x6c,0x40,0x40}, // 80 P
    {0x38,0x44,0x44,0x64,0x5c}, // 81 Q
    {0x6c,0x54,0x6c,0x4c,0x44}, // 82 R
    {0x38,0x40,0x38,0x04,0x7c}, // 83 S
    {0x7c,0x44,0x38,0x44,0x44}, // 84 T
    {0x44,0x44,0x44,0x44,0x7c}, // 85 U
    {0x44,0x44,0x28,0x28,0x10}, // 86 V
    {0x44,0x44,0x54,0x54,0x6c}, // 87 W
    {0x44,0x28,0x10,0x28,0x44}, // 88 X
    {0x44,0x28,0x10,0x10,0x10}, // 89 Y
    {0x7c,0x08,0x10,0x20,0x7c}, // 90 Z
    {0x10,0x20,0x40,0x00,0x00}, // 91 [
    {0x20,0x10,0x08,0x04,0x02}, // 92 \ 
    {0x40,0x20,0x10,0x00,0x00}, // 93 ]
    {0x10,0x54,0x10,0x00,0x00}, // 94 ^
    {0x00,0x00,0x00,0x00,0x7c}, // 95 _
    {0x04,0x04,0x08,0x00,0x00}, // 96 `
    
    // 96-127: autres caractères...
    {0x18,0x18,0x18,0x00,0x00}, // 96 `
    {0x00,0x38,0x04,0x3c,0x44}, // 97 a
    {0x40,0x38,0x44,0x38,0x00}, // 98 b
    {0x00,0x3c,0x40,0x3c,0x00}, // 99 c
    {0x04,0x38,0x44,0x38,0x40}, // 100 d
    {0x00,0x38,0x44,0x3c,0x00}, // 101 e
    {0x00,0x18,0x20,0x20,0x20}, // 102 f
    {0x00,0x3c,0x04,0x38,0x04}, // 103 g
    {0x40,0x38,0x44,0x44,0x00}, // 104 h
    {0x00,0x00,0x44,0x00,0x00}, // 105 i
    {0x04,0x00,0x04,0x04,0x38}, // 106 j
    {0x40,0x40,0x48,0x50,0x00}, // 107 k
    {0x00,0x00,0x00,0x00,0x00}, // 108 l
    {0x00,0x44,0x6c,0x44,0x00}, // 109 m
    {0x00,0x44,0x6c,0x40,0x00}, // 110 n
    {0x00,0x38,0x44,0x38,0x00}, // 111 o
    {0x00,0x38,0x44,0x38,0x40}, // 112 p
    {0x00,0x38,0x44,0x3c,0x04}, // 113 q
    {0x00,0x54,0x58,0x44,0x00}, // 114 r
    {0x00,0x3c,0x40,0x3c,0x00}, // 115 s
    {0x00,0x20,0x3c,0x20,0x00}, // 116 t
    {0x00,0x44,0x44,0x38,0x00}, // 117 u
    {0x00,0x44,0x44,0x28,0x00}, // 118 v
    {0x00,0x44,0x54,0x54,0x00}, // 119 w
    {0x00,0x44,0x28,0x44,0x00}, // 120 x
    {0x00,0x44,0x28,0x10,0x00}, // 121 y
    {0x00,0x3c,0x10,0x3c,0x00}, // 122 z
    {0x10,0x20,0x40,0x00,0x00}, // 123 {
    {0x00,0x10,0x10,0x10,0x00}, // 124 |
    {0x40,0x20,0x10,0x00,0x00}, // 125 }
    {0x20,0x40,0x20,0x00,0x00}  // 126 ~
};

// Fonctions graphiques de base
static void fill_rect(int x, int y, int w, int h, unsigned char c) {
    for (int yy = y; yy < y + h && yy < SCREEN_H; yy++) {
        for (int xx = x; xx < x + w && xx < SCREEN_W; xx++) {
            gfx_pixel(xx, yy, c);
        }
    }
}

static void draw_char_pixel(int x, int y, char c, unsigned char col) {
    int idx = c;
    if (idx < 0 || idx >= 128) return;
    
    for (int row = 0; row < 5; row++) {
        unsigned char bits = font[idx][row];
        for (int col = 0; col < 3; col++) {
            if (bits & (0x10 >> col)) {
                gfx_pixel(x + col, y + row, col);
            }
        }
    }
}

static void draw_text_small(int x, int y, const char* text, unsigned char col) {
    for (int i = 0; text[i]; i++) {
        draw_char_pixel(x + i * 4, y, text[i], col);
    }
}

static void draw_frame(int x, int y, int w, int h, unsigned char col) {
    fill_rect(x, y, w, 1, col);
    fill_rect(x, y + h - 1, w, 1, col);
    fill_rect(x, y, 1, h, col);
    fill_rect(x + w - 1, y, 1, h, col);
}

static void draw_icon(int x, int y, int type) {
    switch (type) {
        case 0: // Dossier
            fill_rect(x+4, y+4, 24, 16, COL_ICON_NORMAL);
            fill_rect(x+6, y+2, 20, 4, COL_ICON_NORMAL);
            fill_rect(x+10, y+0, 12, 2, COL_ICON_NORMAL);
            break;
        case 1: // Fichier texte
            fill_rect(x+4, y+8, 24, 20, COL_ICON_NORMAL);
            fill_rect(x+2, y+6, 28, 4, COL_ICON_NORMAL);
            for (int i = 0; i < 4; i++) {
                for (int j = 0; j < 3; j++) {
                    gfx_pixel(x+6+j*8, y+12+i*4, COL_TEXT_NORMAL);
                    gfx_pixel(x+8+j*8, y+12+i*4, COL_TEXT_NORMAL);
                    gfx_pixel(x+10+j*8, y+12+i*4, COL_TEXT_NORMAL);
                }
            }
            break;
        case 2: // Exécutable
            fill_rect(x+8, y+4, 16, 24, COL_ICON_NORMAL);
            fill_rect(x+10, y+6, 12, 12, COL_TEXT_NORMAL);
            fill_rect(x+12, y+8, 8, 2, COL_ICON_NORMAL);
            break;
        default: // Inconnu
            fill_rect(x+4, y+4, 24, 24, COL_ICON_NORMAL);
            fill_rect(x+14, y+6, 4, 2, COL_TEXT_NORMAL);
            fill_rect(x+14, y+10, 4, 2, COL_TEXT_NORMAL);
            fill_rect(x+14, y+14, 4, 2, COL_TEXT_NORMAL);
            fill_rect(x+16, y+16, 4, 6, COL_TEXT_NORMAL);
            break;
    }
}

static void draw_cursor(int x, int y) {
    // Curseur style flèche
    gfx_pixel(x, y, COL_CURSOR);
    gfx_pixel(x+1, y, COL_CURSOR);
    gfx_pixel(x+2, y, COL_CURSOR);
    gfx_pixel(x+1, y-1, COL_CURSOR);
    gfx_pixel(x+1, y+1, COL_CURSOR);
    gfx_pixel(x+1, y+2, COL_CURSOR);
}

// Structure pour les fichiers
struct FileEntry {
    const char* name;
    const char* ext;
    int type;  // 0=dir, 1=text, 2=exe, 3=unknown
    int size;
};

static FileEntry current_dir[] = {
    {"SYSTEM", "", 0, 0},
    {"Programs", "", 0, 0},
    {"HELLO", "TXT", 1, 1024},
    {"README", "TXT", 1, 2048},
    {"CALC", "EXE", 2, 8192},
    {"EDITOR", "EXE", 2, 16384},
    {"CONFIG", "INI", 3, 512},
    {"TEMP", "", 0, 0}
};

static const int FILE_COUNT = sizeof(current_dir) / sizeof(current_dir[0]);

// État de l'application
struct DesktopState {
    int mouse_x;
    int mouse_y;
    int selected;
    int click_timer;
    char status[128];
};

static int get_item_at_position(int mouse_x, int mouse_y) {
    int content_y = WINDOW_Y + TITLE_H + 8;
    int start = 0;  // Simple, pas de scroll
    int end = min(6, FILE_COUNT);  // 6 items max visibles
    
    for (int i = start; i < end; i++) {
        int item_y = content_y + (i - start) * 20;
        int item_x = WINDOW_X + 8;
        
        if (mouse_x >= item_x - 4 && mouse_x < item_x + WINDOW_W - 16 &&
            mouse_y >= item_y - 4 && mouse_y < item_y + 16) {
            return i;
        }
    }
    return -1;
}

static void render_window(DesktopState* desk) {
    // Fond
    fill_rect(WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H, COL_WINDOW_BG);
    
    // Bordure
    draw_frame(WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H, COL_WINDOW_FRAME);
    
    // Barre de titre
    fill_rect(WINDOW_X + 1, WINDOW_Y + 1, WINDOW_W - 2, TITLE_H - 1, COL_TITLE_BAR);
    draw_text_small(WINDOW_X + 6, WINDOW_Y + 6, "DOS64 File Manager", COL_TITLE_TEXT);
    
    // Bouton de fermeture
    fill_rect(WINDOW_X + WINDOW_W - 20, WINDOW_Y + 3, 14, 12, COL_WINDOW_FRAME);
    draw_text_small(WINDOW_X + WINDOW_W - 18, WINDOW_Y + 5, "X", COL_TITLE_TEXT);
    
    // Zone de contenu
    int content_y = WINDOW_Y + TITLE_H + 8;
    int visible_count = min(6, FILE_COUNT);
    
    for (int i = 0; i < visible_count; i++) {
        int item_y = content_y + i * 20;
        int item_x = WINDOW_X + 8;
        
        // Survol
        if (i == get_item_at_position(desk->mouse_x, desk->mouse_y)) {
            fill_rect(item_x - 4, item_y - 4, WINDOW_W - 16, 16, COL_DESKTOP);
        }
        
        // Sélection
        if (i == desk->selected) {
            fill_rect(item_x - 4, item_y - 4, WINDOW_W - 16, 16, COL_ICON_SEL);
        }
        
        // Icône
        draw_icon(item_x, item_y, current_dir[i].type);
        
        // Nom du fichier
        char display_name[32];
        strcpy(display_name, current_dir[i].name);
        if (current_dir[i].ext[0]) {
            strcat(display_name, ".");
            strcat(display_name, current_dir[i].ext);
        }
        
        draw_text_small(item_x + 40, item_y + 8, display_name, 
                       i == desk->selected ? COL_TEXT_SEL : COL_TEXT_NORMAL);
        
        // Taille
        if (current_dir[i].type != 0 && current_dir[i].size > 0) {
            char size_str[16];
            if (current_dir[i].size < 1024) {
                itoa(current_dir[i].size, size_str);
                strcat(size_str, "B");
            } else {
                itoa(current_dir[i].size / 1024, size_str);
                strcat(size_str, "K");
            }
            draw_text_small(item_x + 200, item_y + 8, size_str, 
                           i == desk->selected ? COL_TEXT_SEL : COL_TEXT_NORMAL);
        }
    }
    
    // Barre de statut
    fill_rect(WINDOW_X + 1, WINDOW_Y + WINDOW_H - 20, WINDOW_W - 2, 19, COL_WINDOW_FRAME);
    fill_rect(WINDOW_X + 2, WINDOW_Y + WINDOW_H - 19, WINDOW_W - 4, 17, COL_WINDOW_BG);
    draw_text_small(WINDOW_X + 6, WINDOW_Y + WINDOW_H - 14, desk->status, COL_TEXT_NORMAL);
}

static void render_desktop(DesktopState* desk) {
    // Fond
    fill_rect(0, 0, SCREEN_W, SCREEN_H, COL_DESKTOP);
    
    // Fenêtre
    render_window(desk);
    
    // Curseur
    draw_cursor(desk->mouse_x, desk->mouse_y);
}

static void open_item(int index, DesktopState* desk) {
    if (index < 0 || index >= FILE_COUNT) return;
    
    FileEntry* item = &current_dir[index];
    
    if (item->type == 0) {
        strcpy(desk->status, "Dossier: ");
        strcat(desk->status, item->name);
    } else if (item->type == 2) {
        strcpy(desk->status, "Exécutable: ");
        strcat(desk->status, item->name);
    } else {
        strcpy(desk->status, "Fichier: ");
        strcat(desk->status, item->name);
    }
}

static void handle_mouse(DesktopState* desk) {
    // Mettre à jour la position de la souris
    desk->mouse_x = pointer_x();
    desk->mouse_y = pointer_y();
    int button = -1;
    
    // Limiter à l'écran
    if (desk->mouse_x < 0) desk->mouse_x = 0;
    if (desk->mouse_x >= SCREEN_W) desk->mouse_x = SCREEN_W - 1;
    if (desk->mouse_y < 0) desk->mouse_y = 0;
    if (desk->mouse_y >= SCREEN_H) desk->mouse_y = SCREEN_H - 1;
    
    // Bouton de fermeture
    if (button && desk->mouse_x >= WINDOW_X + WINDOW_W - 20 && 
        desk->mouse_x < WINDOW_X + WINDOW_W - 6 &&
        desk->mouse_y >= WINDOW_Y + 3 && 
        desk->mouse_y < WINDOW_Y + 15) {
        desk->status[0] = '\0';
        strcpy(desk->status, "Fermeture...");
    }
    
    // Clic sur un fichier
    if (button) {
        int clicked_item = get_item_at_position(desk->mouse_x, desk->mouse_y);
        if (clicked_item >= 0) {
            if (desk->click_timer > 0) {
                // Double-clic
                open_item(clicked_item, desk);
                desk->click_timer = 0;
            } else {
                // Simple clic
                desk->selected = clicked_item;
                desk->click_timer = DOUBLE_CLICK_TIME;
            }
        }
    }
    
    // Gérer le timer de double-clic
    if (desk->click_timer > 0) {
        desk->click_timer--;
    }
}

static void handle_keyboard(char c, DesktopState* desk) {
    switch (c) {
        case 27:
        case 'q':
        case 'Q':
            desk->status[0] = '\0';
            strcpy(desk->status, "Quitter...");
            break;
            
        case '\r':
        case ' ':
        case 'e':
        case 'E':
            if (desk->selected >= 0) {
                open_item(desk->selected, desk);
            }
            break;
            
        case 'w':
        case 'W':
        case 'k':
        case 'K':
            if (desk->selected > 0) {
                desk->selected--;
            }
            break;
            
        case 's':
        case 'S':
        case 'j':
        case 'J':
            if (desk->selected < FILE_COUNT - 1) {
                desk->selected++;
            }
            break;
            
        default:
            break;
    }
}

extern "C" int main() {
    gfx_init();
    
    DesktopState desk;
    desk.mouse_x = SCREEN_W / 2;
    desk.mouse_y = SCREEN_H / 2;
    desk.selected = 0;
    desk.click_timer = 0;
    strcpy(desk.status, "Clic ou double-clic sur un fichier | ESC pour quitter");
    
    while (true) {
        // Gérer la souris
        handle_mouse(&desk);
        
        // Gérer le clavier
        char c = getchar();
        if (c) {
            handle_keyboard(c, &desk);
        }
        
        // Rendre l'écran
        render_desktop(&desk);
        
        // Sortie si demandé
        if (strcmp(desk.status, "Fermeture...") == 0) {
            break;
        }
        
        // Petit délai
        for (volatile int i = 0; i < 1000; i++);
    }
    
    // Écran de sortie
    fill_rect(0, 0, SCREEN_W, SCREEN_H, COL_WINDOW_BG);
    draw_text_small(SCREEN_W/2 - 30, SCREEN_H/2 - 5, "MERCI", COL_TEXT_NORMAL);
    draw_text_small(SCREEN_W/2 - 30, SCREEN_H/2 + 5, "DOS64", COL_TEXT_NORMAL);
    
    gfx_exit();
    return 0;
}
