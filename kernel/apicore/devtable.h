#pragma once
// table de périphériques enregistrés par les drivers
// le kernel peut ensuite les récupérer par nom, ou les lister dans une commande dédiée
#include "kapi.h"
#include "../standart.h"
#include "../prompt.h"
struct Device {
    char name[32];
    void* ptr;
    int   type;
    bool  active;
};

static const int MAX_DEVICES = 1080;

class DeviceTable {
    Device devices[MAX_DEVICES];
    int    count = 0;

public:
    void init() {
        for (int i = 0; i < MAX_DEVICES; i++)
            devices[i].active = false;
    }

    void register_device(const char* name, void* ptr, int type) {
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (devices[i].active) continue;
            int j = 0;
            for (; name[j] && j < 31; j++) devices[i].name[j] = name[j];
            devices[i].name[j] = '\0';
            devices[i].ptr    = ptr;
            devices[i].type   = type;
            devices[i].active = true;
            count++;
            return;
        }
    }

    void* get(const char* name) {
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (!devices[i].active) continue;
            if (strcmp(devices[i].name, name) == 0)
                return devices[i].ptr;
        }
        return nullptr;
    }

    void list(Terminal* term) {
        term->set_color(0xB);
        term->println("=== Registered Devices ===");
        term->set_color(0xF);
        for (int i = 0; i < MAX_DEVICES; i++) {
            if (!devices[i].active) continue;
            const char* type_str =
                devices[i].type == DEV_AUDIO   ? "AUDIO"   :
                devices[i].type == DEV_VIDEO   ? "VIDEO"   :
                devices[i].type == DEV_NETWORK ? "NETWORK" :
                devices[i].type == DEV_INPUT   ? "INPUT"   :
                devices[i].type == DEV_STORAGE ? "STORAGE" : "UNKNOWN";
            term->print("  [");
            term->print(type_str);
            term->print("] ");
            term->println(devices[i].name);
        }
        if (!count) term->println("  No devices registered.");
    }
};