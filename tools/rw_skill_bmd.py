r"""Anade (o actualiza) las 7 skills del Rune Wizard (630-636) en skill_eng.bmd.

Uso: python tools/rw_skill_bmd.py [ruta\skill_eng.bmd]

El formato es el de SKILL_ATTRIBUTE_FILE (108 bytes por registro, Bux por registro, checksum de 4 bytes con la clave 0x5A18).
Cada fila nueva copia una skill de S6 que se comporta igual (para el tipo de uso, la clase, el efecto) y cambia el nombre, las cifras
y el icono. Es idempotente: se puede ejecutar varias veces.
"""
import os
import struct
import sys

RECORD = 108
COUNT = 650
BUX = (0xFC, 0xCF, 0xAB)
CHECKSUM_KEY = 0x5A18

# Offsets del registro (ver SkillFieldDefs.h, con el relleno de la estructura)
NAME, NAME_LEN = 0, 50
LEVEL, DAMAGE, MANA, AG = 50, 52, 54, 56
DISTANCE, DELAY, ENERGY = 60, 64, 68
MAGIC_ICON, IS_DAMAGE = 92, 105
REQUIRE_CLASS = 84      # siete bytes (DW, DK, ELF, MG, DL, SU, RF); el RW usa la clase base de DW
CLASS_ANY_CHANGE = 1

# numero: (nombre, plantilla de S6, danio, mana, AG, distancia, energia, icono de S6, es danio)
SKILLS = {
    630: ('Magic Arrow', 17, 20, 4, 0, 6, 30, 17, 1),
    631: ('Plasma Ball', 11, 38, 30, 0, 6, 120, 11, 1),
    632: ('Lightning Storm', 39, 70, 80, 0, 6, 300, 3, 1),
    633: ('Burst', 233, 0, 90, 30, 6, 200, 233, 0),
    634: ('Haste', 48, 0, 80, 30, 6, 200, 48, 0),
    635: ('Runic Spear', 13, 55, 45, 0, 5, 220, 2, 1),
    636: ('Rune Phrase', 9, 95, 120, 0, 6, 400, 9, 1),
}


def bux(data):
    return bytes(b ^ BUX[i % 3] for i, b in enumerate(data))


def checksum(buf, key=CHECKSUM_KEY):
    result = (key << 9) & 0xFFFFFFFF
    for offset in range(0, len(buf) - 3, 4):
        word = struct.unpack_from('<I', buf, offset)[0]
        if ((offset // 4) + key) % 2 == 0:
            result ^= word
        else:
            result = (result + word) & 0xFFFFFFFF
        if offset % 16 == 0:
            result ^= ((key + result) & 0xFFFFFFFF) >> ((offset // 4) % 8 + 1)
    return result


def main(path):
    data = open(path, 'rb').read()
    body, stored = data[:-4], struct.unpack('<I', data[-4:])[0]
    if len(body) != RECORD * COUNT or checksum(body) != stored:
        sys.exit('skill_eng.bmd no tiene el formato esperado (650 x 108 bytes con checksum correcto)')

    records = [bytearray(bux(body[i * RECORD:(i + 1) * RECORD])) for i in range(COUNT)]
    for number, (name, template, damage, mana, ag, distance, energy, icon_of, is_damage) in SKILLS.items():
        record = bytearray(records[template])
        record[NAME:NAME + NAME_LEN] = name.encode('utf-8').ljust(NAME_LEN, b'\0')
        struct.pack_into('<H', record, LEVEL, 1)
        struct.pack_into('<H', record, DAMAGE, damage)
        struct.pack_into('<H', record, MANA, mana)
        struct.pack_into('<H', record, AG, ag)
        struct.pack_into('<I', record, DISTANCE, distance)
        struct.pack_into('<i', record, ENERGY, energy)
        struct.pack_into('<H', record, MAGIC_ICON, struct.unpack_from('<H', records[icon_of], MAGIC_ICON)[0])
        record[IS_DAMAGE] = is_damage
        record[REQUIRE_CLASS:REQUIRE_CLASS + 7] = bytes([CLASS_ANY_CHANGE, 0, 0, 0, 0, 0, 0])
        records[number] = record

    out = b''.join(bux(bytes(r)) for r in records)
    open(path, 'wb').write(out + struct.pack('<I', checksum(out)))
    print('skills 630-636 escritas en', path)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'bin', 'Data', 'Local', 'Eng', 'skill_eng.bmd'))
