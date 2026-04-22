#pragma once
#include "io.h"
// je crois que le driver ATA est mauvais, mieux vaut switch vers SATA ou SCSI dès que possible.



// Ports ATA primaire
static const unsigned short ATA_DATA         = 0x1F0;
static const unsigned short ATA_ERROR        = 0x1F1;
static const unsigned short ATA_SECTOR_COUNT = 0x1F2;
static const unsigned short ATA_LBA_LOW      = 0x1F3;
static const unsigned short ATA_LBA_MID      = 0x1F4;
static const unsigned short ATA_LBA_HIGH     = 0x1F5;
static const unsigned short ATA_DRIVE_HEAD   = 0x1F6;
static const unsigned short ATA_STATUS       = 0x1F7;
static const unsigned short ATA_COMMAND      = 0x1F7;

// Bits de statut
static const unsigned char ATA_SR_BSY  = 0x80;  // Busy
static const unsigned char ATA_SR_DRDY = 0x40;  // Drive ready
static const unsigned char ATA_SR_DRQ  = 0x08;  // Data request
static const unsigned char ATA_SR_ERR  = 0x01;  // Error

// Commandes ATA
static const unsigned char ATA_CMD_READ  = 0x20;  // Read sectors PIO
static const unsigned char ATA_CMD_WRITE = 0x30;  // Write sectors PIO
static const unsigned char ATA_CMD_ID    = 0xEC;  // Identify

class ATADriver {
    bool present = false;
    char model[41];  // Nom du disque

    // Attendre que le disque soit prêt
    bool wait_ready() {
        for (int timeout = 0; timeout < 100000; timeout++) {
            unsigned char status = inb(ATA_STATUS);
            if (status & ATA_SR_ERR)  return false;  // Erreur
            if (status & ATA_SR_BSY)  continue;       // Occupé
            if (status & ATA_SR_DRDY) return true;    // Prêt
        }
        return false;  // Timeout
    }

    // Attendre DRQ (données prêtes)
    bool wait_drq() {
    for (int timeout = 0; timeout < 1000; timeout++) {  // ← réduit à 1000
        ata_delay();
        unsigned char status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR) return false;
        if (status & ATA_SR_DRQ) return true;
    }
    return false;  // timeout propre
    }

    // Swap les paires d'octets (format ATA pour les strings)
    void swap_string(char* str, int len) {
        for (int i = 0; i < len; i += 2) {
            char tmp = str[i];
            str[i]   = str[i+1];
            str[i+1] = tmp;
        }
        // Trim trailing spaces
        for (int i = len - 1; i >= 0 && str[i] == ' '; i--)
            str[i] = '\0';
    }
    // 400 ns delay (4 lectures du port de statut) après certaines commandes, mais on peut faire ça à la main dans les fonctions de lecture/écriture, pas besoin d'une fonction dédiée
    void ata_delay() {
    inb(0x3F6);  // Alternate status register
    inb(0x3F6);
    inb(0x3F6);
    inb(0x3F6);
    }

public:
    // Détecter et initialiser le disque
    bool init() {
    // Reset du controller ATA
    outb(0x3F6, 0x04);  // Software reset
    ata_delay();
    outb(0x3F6, 0x00);  // Reset off
    ata_delay();

    // Sélectionner master drive
    outb(ATA_DRIVE_HEAD, 0xA0);
    ata_delay();          // ← délai obligatoire après sélection

    // Envoyer IDENTIFY
    outb(ATA_COMMAND, ATA_CMD_ID);
    ata_delay();          // ← délai obligatoire après commande

    // Vérifier si le disque existe
    unsigned char status = inb(ATA_STATUS);
    if (status == 0x00 || status == 0xFF) {
        present = false;
        return false;
    }

    if (!wait_drq()) {
        present = false;
        return false;
    }

    // Lire les 256 mots
    static unsigned short id[256];
    for (int i = 0; i < 256; i++)
        id[i] = inw(ATA_DATA);

    for (int i = 0; i < 20; i++) {
        model[i*2]   = (id[27+i] >> 8) & 0xFF;
        model[i*2+1] = id[27+i] & 0xFF;
    }
    model[40] = '\0';
    swap_string(model, 40);

    present = true;
    return true;
}

    // Lire des secteurs (LBA28)
    // lba    = adresse du secteur (0 = premier secteur)
    // count  = nombre de secteurs à lire (1 secteur = 512 octets)
    // buffer = destination en mémoire
    bool read(unsigned int lba, unsigned char count, void* buffer) {
        if (!present) return false;

        if (!wait_ready()) return false;

        // Configurer LBA
        outb(ATA_DRIVE_HEAD,   0xE0 | ((lba >> 24) & 0x0F));  // LBA mode + bits 24-27
        outb(ATA_SECTOR_COUNT, count);
        outb(ATA_LBA_LOW,      lba & 0xFF);
        outb(ATA_LBA_MID,      (lba >> 8)  & 0xFF);
        outb(ATA_LBA_HIGH,     (lba >> 16) & 0xFF);
        outb(ATA_COMMAND,      ATA_CMD_READ);

        unsigned short* buf = (unsigned short*)buffer;

        for (int s = 0; s < count; s++) {
            if (!wait_drq()) return false;

            // Lire 256 mots (= 512 octets = 1 secteur)
            for (int i = 0; i < 256; i++)
                buf[s * 256 + i] = inw(ATA_DATA);
        }
        return true;
    }

    // Écrire des secteurs (LBA28)
    bool write(unsigned int lba, unsigned char count, const void* buffer) {
        if (!present) return false;

        if (!wait_ready()) return false;

        outb(ATA_DRIVE_HEAD,   0xE0 | ((lba >> 24) & 0x0F));
        outb(ATA_SECTOR_COUNT, count);
        outb(ATA_LBA_LOW,      lba & 0xFF);
        outb(ATA_LBA_MID,      (lba >> 8)  & 0xFF);
        outb(ATA_LBA_HIGH,     (lba >> 16) & 0xFF);
        outb(ATA_COMMAND,      ATA_CMD_WRITE);

        const unsigned short* buf = (const unsigned short*)buffer;

        for (int s = 0; s < count; s++) {
            if (!wait_drq()) return false;

            for (int i = 0; i < 256; i++)
                outw(ATA_DATA, buf[s * 256 + i]);

            // Flush le cache
            outb(ATA_COMMAND, 0xE7);
            wait_ready();
        }
        return true;
    }

    bool is_present()      { return present; }
    const char* get_model() { return model; }
};