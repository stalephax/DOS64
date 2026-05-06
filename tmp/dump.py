#!/usr/bin/env python3
# scan_opcodes.py — lister tous les opcodes uniques d'un EXE DOS
import struct, sys

def scan_mz(filename):
    with open(filename, 'rb') as f:
        data = f.read()

    # Vérifier signature MZ
    if data[0:2] != b'MZ':
        print("Pas un fichier MZ !")
        return

    # Lire le header
    header_para = struct.unpack_from('<H', data, 8)[0]
    code_offset  = header_para * 16
    code         = data[code_offset:]

    print(f"Fichier       : {filename}")
    print(f"Taille totale : {len(data)} bytes")
    print(f"Code offset   : 0x{code_offset:X}")
    print(f"Code size     : {len(code)} bytes")
    print()

    # Compter les opcodes
    opcode_count = {}
    for byte in code:
        opcode_count[byte] = opcode_count.get(byte, 0) + 1

    # Trier par fréquence
    sorted_ops = sorted(opcode_count.items(), key=lambda x: -x[1])

    print("=== Opcodes présents (par fréquence) ===")
    for op, count in sorted_ops:
        print(f"  0x{op:02X}  ({count:6} fois)")

    print()
    print(f"Total opcodes uniques : {len(opcode_count)}")

if __name__ == '__main__':
    scan_mz(sys.argv[1] if len(sys.argv) > 1 else 'WORD.EXE')