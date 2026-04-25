//base SB16 audio
#pragma once
#include "io.h"

class SB16 {
    static const unsigned short BASE = 0x220;  // port de base SB16
    static const unsigned short DSP_RESET  = BASE + 0x6;
    static const unsigned short DSP_READ   = BASE + 0xA;
    static const unsigned short DSP_WRITE  = BASE + 0xC;
    static const unsigned short DSP_STATUS = BASE + 0xE;
    static const unsigned short MIXER_ADDR = BASE + 0x4;
    static const unsigned short MIXER_DATA = BASE + 0x5;

    bool present = false;

    void dsp_write(unsigned char val) {
        for (int i = 0; i < 100000; i++)
            if (!(inb(DSP_WRITE) & 0x80)) break;
        outb(DSP_WRITE, val);
    }

    unsigned char dsp_read() {
        for (int i = 0; i < 100000; i++)
            if (inb(DSP_STATUS) & 0x80) break;
        return inb(DSP_READ);
    }

    void mixer_write(unsigned char reg, unsigned char val) {
        outb(MIXER_ADDR, reg);
        outb(MIXER_DATA, val);
    }

public:
    bool init() {
        // Reset DSP
        outb(DSP_RESET, 1);
        for (volatile int i = 0; i < 100; i++);
        outb(DSP_RESET, 0);

        // Vérifier la réponse (doit retourner 0xAA)
        unsigned char resp = dsp_read();
        if (resp != 0xAA) {
            present = false;
            return false;
        }

        // Lire la version DSP
        dsp_write(0xE1);
        unsigned char major = dsp_read();
        unsigned char minor = dsp_read();
        // SB16 = version 4.x
        if (major < 4) {
            present = false;
            return false;
        }

        // Volume master à 75%
        mixer_write(0x22, 0xCC);  // master volume
        mixer_write(0x26, 0xCC);  // DAC volume

        present = true;
        return true;
    }

    // Jouer un beep via SB16 (sample 8bit mono)
    void play_sample(unsigned char* data, unsigned int size, unsigned int sample_rate) {
        if (!present) return;

        // Programmer la fréquence d'échantillonnage
        dsp_write(0x41);  // Set output sample rate
        dsp_write((sample_rate >> 8) & 0xFF);
        dsp_write(sample_rate & 0xFF);

        // Mode 8 bit mono single cycle
        dsp_write(0xC0);
        dsp_write(0x00);  // mono, unsigned 8bit
        unsigned int len = size - 1;
        dsp_write(len & 0xFF);
        dsp_write((len >> 8) & 0xFF);
    }

    bool is_present() { return present; }
};