#!/usr/bin/env python3
"""Generates the semen decal textures used by PPB_OStrands.esp.

    semen_decal.dds    512x512 BGRA8, 2x2 atlas of four different splats (the engine picks one per decal).
                       RGB = off-white fluid, A = coverage (soft-edged, thin at the rim, thick in the pools).
    semen_decal_n.dds  matching tangent-space normal map; A = specular strength (wet = shiny).

Uncompressed A8R8G8B8 with a full mip chain - the same pixel format OStrands ships its own textures in.
Deterministic (fixed seed), so re-running it reproduces the committed files byte for byte.

Usage:  python3 make_decal_textures.py <output folder>
"""
import os
import struct
import sys

import numpy as np

SIZE = 512
CELL = SIZE // 2


def splat(rng: np.random.Generator, n: int) -> np.ndarray:
    """Height field of one splat: a main blob, satellite droplets and a run or two."""
    y, x = np.mgrid[0:n, 0:n].astype(np.float32) / n
    h = np.zeros((n, n), np.float32)

    def blob(cx, cy, r, amp):
        d2 = ((x - cx) ** 2 + (y - cy) ** 2) / (r * r)
        h[:] += amp * np.exp(-d2 * 2.2)

    cx, cy = 0.5 + rng.uniform(-0.05, 0.05), 0.45 + rng.uniform(-0.05, 0.05)
    for _ in range(rng.integers(5, 9)):                       # irregular main body
        blob(cx + rng.normal(0, 0.06), cy + rng.normal(0, 0.06), rng.uniform(0.07, 0.14), rng.uniform(0.7, 1.0))
    for _ in range(rng.integers(8, 16)):                      # flung droplets
        a, r = rng.uniform(0, 2 * np.pi), rng.uniform(0.18, 0.38)
        blob(cx + r * np.cos(a), cy + r * np.sin(a), rng.uniform(0.012, 0.035), rng.uniform(0.6, 1.0))
    for _ in range(rng.integers(1, 3)):                       # a run streaking down the cell
        sx = cx + rng.uniform(-0.08, 0.08)
        length = rng.uniform(0.2, 0.42)
        for t in np.linspace(0, 1, 24):
            blob(sx + 0.015 * np.sin(t * 5), cy + t * length, 0.03 * (1 - 0.6 * t), 0.75)
        blob(sx, cy + length + 0.02, 0.04, 0.9)               # drip bead at the end

    edge = np.minimum.reduce([x, y, 1 - x, 1 - y])            # never bleed into the neighbour cell
    h *= np.clip(edge / 0.04, 0, 1)
    return np.clip(h, 0, 1.6)


def make(seed: int = 7213):
    rng = np.random.default_rng(seed)
    height = np.zeros((SIZE, SIZE), np.float32)
    for cy in range(2):
        for cx in range(2):
            height[cy * CELL:(cy + 1) * CELL, cx * CELL:(cx + 1) * CELL] = splat(rng, CELL)

    alpha = np.clip(height * 1.4, 0, 1) ** 0.8
    alpha[height < 0.06] = 0
    tint = 0.92 + 0.06 * np.clip(height, 0, 1)                # thicker = whiter
    diffuse = np.stack([tint * 0.97, tint * 0.98, tint, alpha], -1)   # R G B A (slightly cool white)

    gy, gx = np.gradient(height * 6.0)
    nrm = np.stack([-gx, -gy, np.ones_like(height)], -1)
    nrm /= np.linalg.norm(nrm, axis=-1, keepdims=True)
    normal = np.concatenate([nrm * 0.5 + 0.5, np.clip(alpha * 1.1, 0, 1)[..., None]], -1)
    return diffuse, normal


def mips(img: np.ndarray):
    level = img
    while True:
        yield level
        if level.shape[0] == 1:
            return
        level = level.reshape(level.shape[0] // 2, 2, level.shape[1] // 2, 2, 4).mean(axis=(1, 3))


def write_dds(path: str, rgba: np.ndarray):
    levels = list(mips(rgba))
    h, w = rgba.shape[:2]
    header = struct.pack('<4sI', b'DDS ', 124)
    header += struct.pack('<IIIIII', 0x1 | 0x2 | 0x4 | 0x8 | 0x1000 | 0x20000, h, w, w * 4, 0, len(levels))
    header += b'\0' * 44
    header += struct.pack('<II4sIIIII', 32, 0x41, b'\0\0\0\0', 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)
    header += struct.pack('<IIIII', 0x1000 | 0x400000 | 0x8, 0, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(header)
        for lv in levels:
            px = np.clip(lv * 255 + 0.5, 0, 255).astype(np.uint8)
            f.write(px[..., [2, 1, 0, 3]].tobytes())          # BGRA in memory = A8R8G8B8


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    os.makedirs(sys.argv[1], exist_ok=True)
    d, n = make()
    write_dds(os.path.join(sys.argv[1], 'semen_decal.dds'), d)
    write_dds(os.path.join(sys.argv[1], 'semen_decal_n.dds'), n)
    print('wrote', sys.argv[1])
