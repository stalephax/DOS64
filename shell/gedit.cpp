// ============================================================
// GEDIT.ELF — Éditeur de texte graphique pour DOS64
// Utilise libdos64 pour tous les appels système
//
// Contrôles :
//   Flèches      — déplacer le curseur
//   Ctrl+S       — sauvegarder
//   Ctrl+L       — charger
//   Ctrl+N       — nouveau fichier (efface tout)
//   ESC          — quitter
// ============================================================
#include "lib/std.h"

// ============================================================
// Constantes
// ============================================================
static const int SCREEN_W   = 320;
static const int SCREEN_H   = 200;
static const int COLS       = 51;   // colonnes de texte
static const int ROWS       = 19;   // lignes de texte
static const int CHAR_W     = 6;    // largeur d'un glyphe
static const int CHAR_H     = 8;    // hauteur d'un glyphe
static const int TEXT_X     = 4;    // marge gauche
static const int TEXT_Y     = 16;   // marge haute (après la barre de titre)
static const int TITLEBAR_H = 12;   // hauteur barre de titre

// Couleurs VGA
static const unsigned char COL_TITLE_BG  = 1;   // bleu foncé
static const unsigned char COL_TITLE_FG  = 15;  // blanc
static const unsigned char COL_BG        = 0;   // noir
static const unsigned char COL_TEXT      = 2;   // vert
static const unsigned char COL_CURSOR    = 14;  // jaune
static const unsigned char COL_STATUS_OK = 10;  // vert clair
static const unsigned char COL_STATUS_ERR= 12;  // rouge clair
static const unsigned char COL_LINENUM   = 8;   // gris

// ============================================================
// Glyphes 5x7 — police bitmap minimale
// ============================================================
static unsigned char glyph_data(char c, int row) {
    // Normaliser en majuscule pour la lookup
    if (c >= 'a' && c <= 'z') c -= 32;

    static const unsigned char glyphs[][7] = {
        // A-Z
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
        {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
        {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,14}, {17,17,17,31,17,17,17},
        {14, 4, 4, 4, 4, 4,14}, { 1, 1, 1, 1,17,17,14},
        {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
        {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
        {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14, 1, 1,30}, {31, 4, 4, 4, 4, 4, 4},
        {17,17,17,17,17,17,14}, {17,17,17,17,17,10, 4},
        {17,17,17,21,21,21,10}, {17,17,10, 4,10,17,17},
        {17,17,10, 4, 4, 4, 4}, {31, 1, 2, 4, 8,16,31},
        // 0-9
        {14,17,19,21,25,17,14}, { 4,12, 4, 4, 4, 4,14},
        {14,17, 1, 2, 4, 8,31}, {30, 1, 1,14, 1, 1,30},
        { 2, 6,10,18,31, 2, 2}, {31,16,16,30, 1, 1,30},
        {14,16,16,30,17,17,14}, {31, 1, 2, 4, 8, 8, 8},
        {14,17,17,14,17,17,14}, {14,17,17,15, 1, 1,14},
        // Ponctuations
        { 0, 0, 0, 0, 0,12,12}, // .
        { 0, 0, 0,31, 0, 0, 0}, // -
        { 0, 0, 0, 0, 0, 0,31}, // _
        { 0,12,12, 0,12,12, 0}, // :
        { 4, 4, 4, 4, 4, 0, 4}, // !
        {14,17, 1, 2, 4, 0, 4}, // ?
        { 1, 2, 4, 8,16, 0, 0}, // /
        { 0, 4, 4,31, 4, 4, 0}, // +
        { 8, 4, 2, 2, 2, 4, 8}, // )
        { 2, 4, 8, 8, 8, 4, 2}, // (
        { 0, 0, 0, 0, 0, 6, 4}, // ,
        { 0,17,10, 4,10,17, 0}, // x (utilisé pour curseur)
    };

    int idx = -1;
    if (c >= 'A' && c <= 'Z') idx = c - 'A';
    else if (c >= '0' && c <= '9') idx = 26 + (c - '0');
    else if (c == '.') idx = 36;
    else if (c == '-') idx = 37;
    else if (c == '_') idx = 38;
    else if (c == ':') idx = 39;
    else if (c == '!') idx = 40;
    else if (c == '?') idx = 41;
    else if (c == '/') idx = 42;
    else if (c == '+') idx = 43;
    else if (c == ')') idx = 44;
    else if (c == '(') idx = 45;
    else if (c == ',') idx = 46;

    if (idx < 0 || row < 0 || row >= 7) return 0;
    return glyphs[idx][row];
}

static void draw_char(int x, int y, char c, unsigned char fg, unsigned char bg) {
    for (int row = 0; row < 7; row++) {
        unsigned char bits = glyph_data(c, row);
        for (int col = 0; col < 5; col++) {
            unsigned char on = (bits >> (4 - col)) & 1;
            gfx_pixel(x + col, y + row, on ? fg : bg);
        }
    }
}

static void draw_text(int x, int y, const char* s,
                      unsigned char fg, unsigned char bg) {
    for (int i = 0; s[i]; i++)
        draw_char(x + i * CHAR_W, y, s[i], fg, bg);
}

static void draw_rect(int x, int y, int w, int h, unsigned char c) {
    for (int yy = y; yy < y + h; yy++)
        for (int xx = x; xx < x + w; xx++)
            gfx_pixel(xx, yy, c);
}

// ============================================================
// État de l'éditeur
// ============================================================
struct Editor {
    char text[ROWS][COLS + 1];  // +1 pour le null terminator
    int  cx, cy;                // position curseur
    char filepath[128];         // chemin du fichier ouvert
    char status[64];            // message de statut
    unsigned char status_color; // couleur du message
    bool modified;              // fichier modifié

    void init(const char* path) {
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++)
                text[y][x] = ' ';
            text[y][COLS] = '\0';
        }
        cx = cy = 0;
        modified = false;
        status_color = COL_STATUS_OK;

        // Copier le chemin
        int i = 0;
        for (; path[i] && i < 127; i++) filepath[i] = path[i];
        filepath[i] = '\0';

        set_status("CTRL+S save | CTRL+L load | CTRL+N new | ESC quit",
                   COL_TITLE_FG);
    }

    void set_status(const char* msg, unsigned char color) {
        int i = 0;
        for (; msg[i] && i < 63; i++) status[i] = msg[i];
        status[i] = '\0';
        status_color = color;
    }

    // Déplacer le curseur
    void move(int dx, int dy) {
        cx += dx; cy += dy;
        if (cx < 0) { cx = 0; }
        if (cx >= COLS) { cx = COLS - 1; }
        if (cy < 0) { cy = 0; }
        if (cy >= ROWS) { cy = ROWS - 1; }
    }

    // Insérer un caractère
    void insert(char c) {
        if (cx < COLS) {
            text[cy][cx] = c;
            move(1, 0);
            modified = true;
        }
    }

    // Backspace
    void backspace() {
        if (cx > 0) {
            move(-1, 0);
            text[cy][cx] = ' ';
            modified = true;
        }
    }

    // Nouvelle ligne
    void newline() {
        cx = 0;
        if (cy < ROWS - 1) cy++;
        modified = true;
    }

    // Nouveau fichier
    void clear() {
        for (int y = 0; y < ROWS; y++)
            for (int x = 0; x < COLS; x++)
                text[y][x] = ' ';
        cx = cy = 0;
        modified = false;
        set_status("New file", COL_STATUS_OK);
    }

    // Sauvegarder
    void save() {
        static char buf[ROWS * (COLS + 1)];
        int p = 0;
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++)
                buf[p++] = text[y][x];
            buf[p++] = '\n';
        }
        long long written = sys_fs_write(filepath, buf, (unsigned long long)p);
        if (written >= 0) {
            modified = false;
            set_status("Saved!", COL_STATUS_OK);
        } else {
            set_status("Save failed!", COL_STATUS_ERR);
        }
    }

    // Charger
    void load() {
        static char buf[ROWS * (COLS + 1) + 16];
        long long r = sys_fs_read(filepath, buf, sizeof(buf) - 1);
        if (r <= 0) {
            set_status("File not found", COL_STATUS_ERR);
            return;
        }
        buf[r] = '\0';

        int p = 0;
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                char c = (p < r) ? buf[p++] : ' ';
                if (c == '\n' || c == '\r') { c = ' '; }
                text[y][x] = c;
            }
            // Avancer jusqu'à la prochaine ligne
            while (p < r && buf[p] != '\n') p++;
            if (p < r) p++;  // skip '\n'
        }
        cx = cy = 0;
        modified = false;
        set_status("Loaded!", COL_STATUS_OK);
    }
};

// ============================================================
// Rendu de l'écran
// ============================================================
static void render(Editor* ed) {
    // Fond général
    draw_rect(0, 0, SCREEN_W, SCREEN_H, COL_BG);

    // Barre de titre
    draw_rect(0, 0, SCREEN_W, TITLEBAR_H, COL_TITLE_BG);
    draw_text(TEXT_X, 2, "GEDIT", COL_TITLE_FG, COL_TITLE_BG);

    // Indicateur de modification
    if (ed->modified)
        draw_text(TEXT_X + 7 * CHAR_W, 2, "*", COL_CURSOR, COL_TITLE_BG);

    // Nom du fichier
    draw_text(TEXT_X + 10 * CHAR_W, 2, ed->filepath,
              COL_TITLE_FG, COL_TITLE_BG);

    // Message de statut à droite
    draw_text(TEXT_X, 2 + CHAR_H, ed->status,
              ed->status_color, COL_TITLE_BG);

    // Numéros de ligne
    for (int y = 0; y < ROWS; y++) {
        char num[4];
        num[0] = '0' + (y + 1) / 10;
        num[1] = '0' + (y + 1) % 10;
        num[2] = ' ';
        num[3] = '\0';
        draw_text(0, TEXT_Y + y * CHAR_H, num,
                  COL_LINENUM, COL_BG);
    }

    // Zone de texte
    for (int y = 0; y < ROWS; y++) {
        for (int x = 0; x < COLS; x++) {
            char c = ed->text[y][x];
            unsigned char fg = COL_TEXT;
            // Surligner la ligne courante
            if (y == ed->cy) fg = 15;
            draw_char(TEXT_X + 3 * CHAR_W + x * CHAR_W,
                      TEXT_Y + y * CHAR_H,
                      c, fg, COL_BG);
        }
    }

    // Curseur
    draw_rect(TEXT_X + 3 * CHAR_W + ed->cx * CHAR_W - 1,
              TEXT_Y + ed->cy * CHAR_H,
              CHAR_W, CHAR_H,
              COL_CURSOR);
    // Réécrire le caractère sous le curseur en noir
    draw_char(TEXT_X + 3 * CHAR_W + ed->cx * CHAR_W,
              TEXT_Y + ed->cy * CHAR_H,
              ed->text[ed->cy][ed->cx],
              COL_BG, COL_CURSOR);

    // Barre de position en bas
    draw_rect(0, SCREEN_H - CHAR_H, SCREEN_W, CHAR_H, COL_TITLE_BG);
    char pos[24];
    pos[0] = 'L'; pos[1] = ':';
    pos[2] = '0' + (ed->cy + 1) / 10;
    pos[3] = '0' + (ed->cy + 1) % 10;
    pos[4] = ' '; pos[5] = 'C'; pos[6] = ':';
    pos[7] = '0' + (ed->cx + 1) / 10;
    pos[8] = '0' + (ed->cx + 1) % 10;
    pos[9] = '\0';
    draw_text(TEXT_X, SCREEN_H - CHAR_H + 1, pos,
              COL_TITLE_FG, COL_TITLE_BG);
}

// ============================================================
// Point d'entrée
// ============================================================
extern "C" int main() {
    static Editor ed;
    ed.init("C:/GEDIT.TXT");

    gfx_init();

    while (true) {
        render(&ed);

        char c = getchar();
        if (!c) continue;

        switch (c) {
            case 27:   // ESC — quitter
                if (ed.modified) {
                    ed.set_status("Unsaved changes! Press ESC again to quit.",
                                  COL_STATUS_ERR);
                    render(&ed);
                    char c2 = getchar();
                    if (c2 != 27) continue;
                }
                goto done;

            case 19:   // Ctrl+S — sauvegarder
                ed.save();
                break;

            case 12:   // Ctrl+L — charger
                ed.load();
                break;

            case 14:   // Ctrl+N — nouveau fichier
                ed.clear();
                break;

            case '\n':
            case '\r':
                ed.newline();
                break;

            case '\b':
                ed.backspace();
                break;

            default:
                if (c >= 32 && c <= 126)
                    ed.insert(c);
                break;
        }
    }

done:
    gfx_exit();
    kms(0);
    return 0;
}