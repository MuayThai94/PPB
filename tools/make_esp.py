#!/usr/bin/env python3
"""Writes PPB_OStrands.esp: the three forms the OStrands bridge needs to put semen decals on NPCs.

    0x800  TXST  PPBOS_SemenDecalTXST   textures\\PPB_OStrands\\semen_decal.dds (+ _n), 2x2 sub-texture atlas
    0x801  IPCT  PPBOS_SemenImpact      decal-only impact (no model, silent) using the texture set
    0x802  IPDS  PPBOS_SemenImpactSet   EMPTY on disk; PPB_OStrands.dll maps every material type to 0x801 at
                                        load, so no Skyrim.esm material FormIDs are hard-coded here
    0x803  MGEF  PPBOS_ClimaxEffect     script-archetype effect with no script: it only marks the cast
    0x804  SPEL  PPBOS_ClimaxPower      "Climax" lesser power (no cooldown, no magicka); the DLL gives it to the
                                        player and climaxes when the player casts it

Master: Skyrim.esm (only for the power's VoiceEquip slot), so the plugin's own forms carry byte 01 in the file.

Usage:  python3 make_esp.py <output.esp>
"""
import struct
import sys

FORM_VERSION = 44          # Skyrim SE / VR record version
HEDR_VERSION = 1.70        # 1.71 is the AE (extended ESL range) header; VR is happiest with 1.70


def sub(sig: bytes, data: bytes) -> bytes:
    assert len(sig) == 4 and len(data) < 0x10000
    return sig + struct.pack('<H', len(data)) + data


def zstr(s: str) -> bytes:
    return s.encode('cp1252') + b'\0'


def record(sig: bytes, form_id: int, payload: bytes, flags: int = 0) -> bytes:
    return sig + struct.pack('<IIIIHH', len(payload), flags, form_id, 0, FORM_VERSION, 0) + payload


def group(label: bytes, records: list) -> bytes:
    body = b''.join(records)
    return b'GRUP' + struct.pack('<I', 24 + len(body)) + label + struct.pack('<iHHI', 0, 0, 0, 0) + body


def decal_data(min_w, max_w, min_h, max_h, depth, shininess, parallax_scale, parallax_passes, flags, rgba):
    # DODT, 0x24 bytes (RE::DECAL_DATA_DATA)
    return struct.pack('<7fbBH4B', min_w, max_w, min_h, max_h, depth, shininess, parallax_scale,
                       parallax_passes, flags, 0, *rgba)


DECAL_ALPHA_BLENDING = 0x02

# Sizes are in game units (1u ~ 1.43 cm): a splat 4-9 u wide reads as a spurt landing on skin.
DODT = decal_data(4.0, 9.0, 4.0, 9.0, 10.0, 60.0, 4.0, 0, DECAL_ALPHA_BLENDING, (255, 255, 255, 255))

OWN = 0x01000000                     # load-order byte of our own forms (one master before us)
TXST_ID, IPCT_ID, IPDS_ID, MGEF_ID, SPEL_ID = (OWN | n for n in (0x800, 0x801, 0x802, 0x803, 0x804))
VOICE_EQUIP = 0x00025BEE              # Skyrim.esm EquipType 'VoiceEquip' - powers go in the shout slot


def build() -> bytes:
    txst = record(b'TXST', TXST_ID,
                  sub(b'EDID', zstr('PPBOS_SemenDecalTXST')) +
                  sub(b'OBND', b'\0' * 12) +
                  sub(b'TX00', zstr('PPB_OStrands\\semen_decal.dds')) +
                  sub(b'TX01', zstr('PPB_OStrands\\semen_decal_n.dds')) +
                  sub(b'DODT', DODT) +
                  sub(b'DNAM', struct.pack('<H', 0)))

    # DATA: effect duration, orientation (0 = surface normal), angle threshold, placement radius,
    #       sound level (2 = silent), flags (0 = HAS decal data), impact result (0 = default), pad
    ipct = record(b'IPCT', IPCT_ID,
                  sub(b'EDID', zstr('PPBOS_SemenImpact')) +
                  sub(b'DATA', struct.pack('<fIffIBBH', 8.0, 0, 15.0, 6.0, 2, 0, 0, 0)) +
                  sub(b'DODT', DODT) +
                  sub(b'DNAM', struct.pack('<I', TXST_ID)))

    ipds = record(b'IPDS', IPDS_ID, sub(b'EDID', zstr('PPBOS_SemenImpactSet')))

    # MGEF DATA, 0x98 bytes. Script archetype, fire and forget, self, silent, no duration/magnitude/area, painless.
    no_av = 0xFFFFFFFF
    mgef_data = struct.pack('<IfIIIHH', 0x00000E00 | 0x04000000, 0.0, 0, no_av, no_av, 0, 0)
    mgef_data += struct.pack('<IfIIIIffffIi', 0, 0.0, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0.0, 1, -1)   # light..primary AV
    mgef_data += struct.pack('<IIIIi', 0, 0, 1, 0, -1)                                         # proj, expl, F&F, self, 2nd AV
    mgef_data += struct.pack('<IIIfIfIIIIIIIff', 0, 0, 0, 0.0, 0, 1.0, 0, 0, 0, 0, 0, 0, 2, 0.0, 0.0)
    assert len(mgef_data) == 0x98, len(mgef_data)
    mgef = record(b'MGEF', MGEF_ID,
                  sub(b'EDID', zstr('PPBOS_ClimaxEffect')) +
                  sub(b'FULL', zstr('Climax')) +
                  sub(b'DATA', mgef_data) +
                  sub(b'DNAM', zstr('Climax now (PPB OStrands).')))

    # SPIT: cost, flags (1 = manual cost -> 0 magicka), type 3 = lesser power, charge time, cast F&F, target self,
    #       cast duration, range, half-cost perk
    spel = record(b'SPEL', SPEL_ID,
                  sub(b'EDID', zstr('PPBOS_ClimaxPower')) +
                  sub(b'OBND', b'\0' * 12) +
                  sub(b'FULL', zstr('Climax')) +
                  sub(b'ETYP', struct.pack('<I', VOICE_EQUIP)) +
                  sub(b'DESC', zstr('Climax now: OStrands semen from your tip, decals on the NPCs it lands on.')) +
                  sub(b'SPIT', struct.pack('<IIIfIIffI', 0, 1, 3, 0.0, 1, 0, 0.0, 0.0, 0)) +
                  sub(b'EFID', struct.pack('<I', MGEF_ID)) +
                  sub(b'EFIT', struct.pack('<fII', 0.0, 0, 0)))

    groups = [group(b'TXST', [txst]), group(b'IPCT', [ipct]), group(b'IPDS', [ipds]),
              group(b'MGEF', [mgef]), group(b'SPEL', [spel])]

    header = record(b'TES4', 0,
                    sub(b'HEDR', struct.pack('<fiI', HEDR_VERSION, 10, 0x805)) +
                    sub(b'CNAM', zstr('PPB OStrands Bridge')) +
                    sub(b'MAST', zstr('Skyrim.esm')) +
                    sub(b'DATA', struct.pack('<Q', 0)) +
                    sub(b'SNAM', zstr('Decal forms for the PPB -> OStrands bridge (PPB_OStrands.dll).')))
    return header + b''.join(groups)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], 'wb') as f:
        f.write(build())
    print('wrote', sys.argv[1])
