#!/usr/bin/env python3
# make_ktt.py — Générateur de fichiers .KTT pour DOS64
# Les caractères accentués sont encodés en Latin-1 (ISO 8859-1)
# pour rester dans la plage 0-255

import struct

def make_ktt(name, normal, shifted, altgr, output):
    # Magic "KTT1"
    header = b'KTT1'
    # Nom du layout — 32 bytes null-padded
    header += name.encode('latin-1').ljust(32, b'\0')
    # Version
    header += struct.pack('<I', 1)

    def char_to_byte(c):
        if isinstance(c, int):
            return c
        # Encoder en Latin-1 — supporte les accents français
        encoded = c.encode('latin-1')
        return encoded[0]

    def make_table(mapping):
        result = bytearray(128)
        for scancode, char in mapping.items():
            if 0 <= scancode < 128:
                val = char_to_byte(char)
                # Tronquer à 127 si > 127 pour la table de base
                # Les accents seront gérés séparément
                result[scancode] = val & 0xFF
        return bytes(result)

    def make_table_full(mapping):
        """Table de 256 bytes pour supporter les accents (Latin-1)"""
        result = bytearray(128)
        for scancode, char in mapping.items():
            if 0 <= scancode < 128:
                val = char_to_byte(char)
                result[scancode] = val
        return bytes(result)

    data = (header
            + make_table_full(normal)
            + make_table_full(shifted)
            + make_table_full(altgr))

    with open(output, 'wb') as f:
        f.write(data)

    print(f"Generated '{output}' — {len(data)} bytes")
    print(f"Layout: {name}")
    print(f"Normal  table: {len(normal)} mappings")
    print(f"Shifted table: {len(shifted)} mappings")
    print(f"AltGr   table: {len(altgr)} mappings")

# ============================================================
# Layout fr_FR (AZERTY)
# Les accents utilisent Latin-1 : é=233 è=232 ç=231 à=224 ù=249
# ============================================================

NORMAL_FR = {
    1:  '\x1b',  # ESC
    2:  '&',
    3:  '\xe9',  # é
    4:  '"',
    5:  "'",
    6:  '(',
    7:  '-',
    8:  '\xe8',  # è
    9:  '_',
    10: '\xe7',  # ç
    11: '\xe0',  # à
    12: ')',
    13: '=',
    14: '\b',    # Backspace
    15: '\t',    # Tab
    16: 'a',
    17: 'z',
    18: 'e',
    19: 'r',
    20: 't',
    21: 'y',
    22: 'u',
    23: 'i',
    24: 'o',
    25: 'p',
    26: '^',
    27: '$',
    28: '\n',    # Enter
    30: 'q',
    31: 's',
    32: 'd',
    33: 'f',
    34: 'g',
    35: 'h',
    36: 'j',
    37: 'k',
    38: 'l',
    39: 'm',
    40: '\xf9',  # ù
    41: '`',
    43: '*',
    44: 'w',
    45: 'x',
    46: 'c',
    47: 'v',
    48: 'b',
    49: 'n',
    50: ',',
    51: ';',
    52: ':',
    53: '!',
    57: ' ',
    # Pavé numérique
    71: '7', 72: '8', 73: '9', 74: '-',
    75: '4', 76: '5', 77: '6', 78: '+',
    79: '1', 80: '2', 81: '3',
    82: '0', 83: '.',
}

SHIFTED_FR = {
    1:  '\x1b',
    2:  '1',
    3:  '2',
    4:  '3',
    5:  '4',
    6:  '5',
    7:  '6',
    8:  '7',
    9:  '8',
    10: '9',
    11: '0',
    12: '\xb0',  # °
    13: '+',
    14: '\b',
    15: '\t',
    16: 'A',
    17: 'Z',
    18: 'E',
    19: 'R',
    20: 'T',
    21: 'Y',
    22: 'U',
    23: 'I',
    24: 'O',
    25: 'P',
    26: '^',
    27: '$',
    28: '\n',
    30: 'Q',
    31: 'S',
    32: 'D',
    33: 'F',
    34: 'G',
    35: 'H',
    36: 'J',
    37: 'K',
    38: 'L',
    39: 'M',
    40: '%',
    41: '~',
    43: '\xb5',  # µ
    44: 'W',
    45: 'X',
    46: 'C',
    47: 'V',
    48: 'B',
    49: 'N',
    50: '?',
    51: '.',
    52: '/',
    53: '!',
    57: ' ',
}

ALTGR_FR = {
    2:  '~',
    3:  '#',
    4:  '{',
    5:  '[',
    6:  '|',
    7:  '`',
    8:  '\\',
    9:  '^',
    10: '@',
    11: ']',
    12: '}',
    13: '=',
    26: '[',
    27: ']',
    # € sur E — Latin-1 n'a pas €, on utilise le code CP1252
    # On met 0 si pas supporté
}

# ============================================================
# Layout en_US (QWERTY) — pour comparaison
# ============================================================

NORMAL_EN = {
    1:  '\x1b', 2: '1', 3: '2', 4: '3', 5: '4', 6: '5',
    7:  '6', 8: '7', 9: '8', 10: '9', 11: '0', 12: '-', 13: '=',
    14: '\b', 15: '\t',
    16: 'q', 17: 'w', 18: 'e', 19: 'r', 20: 't', 21: 'y',
    22: 'u', 23: 'i', 24: 'o', 25: 'p', 26: '[', 27: ']',
    28: '\n',
    30: 'a', 31: 's', 32: 'd', 33: 'f', 34: 'g', 35: 'h',
    36: 'j', 37: 'k', 38: 'l', 39: ';', 40: "'", 41: '`',
    43: '\\',
    44: 'z', 45: 'x', 46: 'c', 47: 'v', 48: 'b', 49: 'n',
    50: 'm', 51: ',', 52: '.', 53: '/', 57: ' ',
}

SHIFTED_EN = {
    1:  '\x1b', 2: '!', 3: '@', 4: '#', 5: '$', 6: '%',
    7:  '^', 8: '&', 9: '*', 10: '(', 11: ')', 12: '_', 13: '+',
    14: '\b', 15: '\t',
    16: 'Q', 17: 'W', 18: 'E', 19: 'R', 20: 'T', 21: 'Y',
    22: 'U', 23: 'I', 24: 'O', 25: 'P', 26: '{', 27: '}',
    28: '\n',
    30: 'A', 31: 'S', 32: 'D', 33: 'F', 34: 'G', 35: 'H',
    36: 'J', 37: 'K', 38: 'L', 39: ':', 40: '"', 41: '~',
    43: '|',
    44: 'Z', 45: 'X', 46: 'C', 47: 'V', 48: 'B', 49: 'N',
    50: 'M', 51: '<', 52: '>', 53: '?', 57: ' ',
}

ALTGR_EN = {}  # QWERTY n'a pas d'AltGr

# ============================================================
# Génération des fichiers
# ============================================================

make_ktt("fr_FR", NORMAL_FR, SHIFTED_FR, ALTGR_FR, "fr_FR.ktt")
make_ktt("en_US", NORMAL_EN, SHIFTED_EN, ALTGR_EN, "en_US.ktt")

print("\nDone! Copy to disk:")
print("  mcopy -i disk.img fr_FR.ktt ::SYSTEM/fr_FR.ktt")
print("  mcopy -i disk.img en_US.ktt ::SYSTEM/en_US.ktt")