#pragma once
#include "../standart.h"

class PathManager {
    char cwd[256];      // répertoire courant ex: "C:/PROGRAMS"
    char volume[4];     // volume courant ex: "C:"

public:
    void init() {
        // Démarrer à la racine de C:
        cwd[0] = '\0';
        volume[0] = 'C';
        volume[1] = ':';
        volume[2] = '\0';
    }

    // Retourne le prompt complet ex: "C:/> " ou "C:/PROGRAMS> "
    void get_prompt(char* out) {
        int i = 0;
        // Volume
        out[i++] = volume[0];
        out[i++] = ':';
        out[i++] = '/';
        // Chemin courant
        if (cwd[0]) {
            for (int j = 0; cwd[j]; j++)
                out[i++] = cwd[j];
            out[i++] = '/';
        }
        out[i++] = '>';
        out[i++] = ' ';
        out[i] = '\0';
    }

    // Retourne le chemin absolu d'un chemin relatif ou absolu
    void resolve(const char* input, char* out) {
        // Chemin absolu avec volume
        if (input[1] == ':') {
            // Copier tel quel en normalisant les séparateurs
            int i = 0;
            for (int j = 0; input[j]; j++) {
                out[i++] = (input[j] == '\\') ? '/' : input[j];
            }
            out[i] = '\0';
            return;
        }

        // Chemin absolu sans volume (commence par / ou \)
        if (input[0] == '/' || input[0] == '\\') {
            int i = 0;
            out[i++] = volume[0];
            out[i++] = ':';
            for (int j = 0; input[j]; j++)
                out[i++] = (input[j] == '\\') ? '/' : input[j];
            out[i] = '\0';
            return;
        }

        // Chemin relatif — préfixer avec le CWD
        int i = 0;
        out[i++] = volume[0];
        out[i++] = ':';
        out[i++] = '/';
        if (cwd[0]) {
            for (int j = 0; cwd[j]; j++)
                out[i++] = cwd[j];
            out[i++] = '/';
        }
        for (int j = 0; input[j]; j++)
            out[i++] = (input[j] == '\\') ? '/' : input[j];
        out[i] = '\0';
    }

    // Changer de répertoire
    // Retourne false si le chemin est invalide
    bool cd(const char* path) {
        // Remonter d'un niveau
        if (strcmp(path, "..") == 0) {
            // Trouver le dernier '/'
            int last = -1;
            for (int i = 0; cwd[i]; i++)
                if (cwd[i] == '/') last = i;

            if (last < 0)
                cwd[0] = '\0';  // déjà à la racine
            else
                cwd[last] = '\0';
            return true;
        }

        // Racine
        if (strcmp(path, "/") == 0 || strcmp(path, "\\") == 0) {
            cwd[0] = '\0';
            return true;
        }

        // Changer de volume (ex: "B:")
        if (path[1] == ':' && path[2] == '\0') {
            volume[0] = path[0];
            cwd[0] = '\0';
            return true;
        }

        // Descendre dans un sous-dossier
        // On vérifie que le chemin ne commence pas par / (absolu)
        char new_cwd[256];
        int i = 0;

        if (cwd[0]) {
            for (int j = 0; cwd[j]; j++)
                new_cwd[i++] = cwd[j];
            new_cwd[i++] = '/';
        }

        // Normaliser et ajouter le nouveau composant
        const char* p = path;
        if (p[0] == '/' || p[0] == '\\') p++;  // ignorer slash initial
        for (int j = 0; p[j]; j++)
            new_cwd[i++] = (p[j] == '\\') ? '/' : p[j];
        new_cwd[i] = '\0';

        // Mettre à jour
        for (int j = 0; j <= i; j++)
            cwd[j] = new_cwd[j];
        return true;
    }

    const char* get_cwd()    { return cwd; }
    const char* get_volume() { return volume; }
};