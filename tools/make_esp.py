#!/usr/bin/env python3
"""Writes PPB_OStrands.esp: the three forms the OStrands bridge needs to put semen decals on NPCs.

    0x800  TXST  PPBOS_SemenDecalTXST   textures\\PPB_OStrands\\semen_decal.dds (+ _n), 2x2 sub-texture atlas
    0x801  IPCT  PPBOS_SemenImpact      decal-only impact (no model, silent) using the texture set
    0x802  IPDS  PPBOS_SemenImpactSet   EMPTY on disk; PPB_OStrands.dll maps every material type to 0x801 at
                                        load, so no Skyrim.esm material FormIDs are hard-coded here

The plugin has no masters, so its own forms carry load-order byte 00 inside the file.

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

TXST_ID, IPCT_ID, IPDS_ID = 0x800, 0x801, 0x802


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

    groups = [group(b'TXST', [txst]), group(b'IPCT', [ipct]), group(b'IPDS', [ipds])]

    header = record(b'TES4', 0,
                    sub(b'HEDR', struct.pack('<fiI', HEDR_VERSION, 6, 0x803)) +
                    sub(b'CNAM', zstr('PPB OStrands Bridge')) +
                    sub(b'SNAM', zstr('Decal forms for the PPB -> OStrands bridge (PPB_OStrands.dll).')))
    return header + b''.join(groups)


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], 'wb') as f:
        f.write(build())
    print('wrote', sys.argv[1])
