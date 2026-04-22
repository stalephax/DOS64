#pragma once
#include "drivers/io.h"

class Beeper {
    // Le PC speaker utilise le timer PIT (8253)
    static const unsigned int PIT_FREQ = 1193180;

    void set_frequency(unsigned int freq) {
        unsigned int divisor = PIT_FREQ / freq;
        outb(0x43, 0xB6);                    // Mode 3, canal 2
        outb(0x42, divisor & 0xFF);          // Octet bas
        outb(0x42, (divisor >> 8) & 0xFF);   // Octet haut
    }

    void speaker_on() {
        unsigned char val = inb(0x61);
        outb(0x61, val | 0x03);  // Activer speaker + gate du timer
    }

    void speaker_off() {
        unsigned char val = inb(0x61);
        outb(0x61, val & ~0x03); // Désactiver
    }

public:
    void beep(unsigned int freq, unsigned int duration_ms) {
        set_frequency(freq);
        speaker_on();
        // Délai approximatif
        for (volatile unsigned int i = 0; i < duration_ms * 10000; i++);
        speaker_off();
    }

    void beep_ok()    { beep(1000, 100); }  // Bip court aigu
    void beep_error() { beep(200,  500); }  // Bip long grave
    void beep_boot()  {                     // Séquence de démarrage
        beep(523, 100);  // Do
        beep(659, 100);  // Mi
        beep(784, 150);  // Sol
    }
};