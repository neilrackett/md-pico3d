#!/usr/bin/env python3
"""
gen_palette.py — Generate 4 day/night palettes and LUTs for Pico3D ST port.

Scans chunk_data.c for all RGB4444 color values, simulates the 4 day/night
lighting phases (applying light_falloff to each phase's colors), then runs
median cut to produce 4 × 16-color palettes. For each palette it builds a
4096-entry LUT mapping every possible RGB4444 value to the nearest palette
index. Outputs palette_data.h to rp/src/include/.

Usage:
    python3 tools/gen_palette.py

Output:
    rp/src/include/palette_data.h
"""

import re
import sys
import os
from pathlib import Path

# ── Color format ─────────────────────────────────────────────────────────────
# Pico3D RGB4444 word layout in game data: bits [15:12]=G, [11:8]=B,
# [7:4]=unused, [3:0]=R (GBAR = g<<12 | b<<8 | 0<<4 | r).
# LUT indexing uses dense packed RGB444 keys: (g<<8 | b<<4 | r), 0..4095.
# ST palette format: 0x0RGB (3 bits per channel: bits [8:6]=R, [5:3]=G, [2:0]=B)
# but stored as uint16_t words for direct write to $FFFF8240.
# Since our C2P uses 4-bit-per-channel, we map to ST's 3-bit range (0..7).

def gbar_word_to_rgb(v):
    """Unpack Pico3D GBAR RGB4444 word to (r,g,b), each channel 0-15."""
    r = v & 0x0F
    b = (v >> 8) & 0x0F
    g = (v >> 12) & 0x0F
    return (r, g, b)

def packed_rgb444_key_to_rgb(key):
    """Unpack LUT key (g<<8 | b<<4 | r) to (r,g,b), each channel 0-15."""
    r = key & 0x0F
    b = (key >> 4) & 0x0F
    g = (key >> 8) & 0x0F
    return (r, g, b)

def rgb4444_to_st_palette_word(r, g, b):
    """Convert (r,g,b) 0-15 to Atari ST hardware palette word (0x0RGB, 3 bits/ch)."""
    # Scale 0-15 to 0-7 (3-bit ST color range)
    r3 = (r >> 1) & 0x7
    g3 = (g >> 1) & 0x7
    b3 = (b >> 1) & 0x7
    return (r3 << 8) | (g3 << 4) | b3

# ── Day/night phases ─────────────────────────────────────────────────────────
# light_falloff values for each phase (subtracted from each R,G,B channel)
# Phase 0=day: falloff=0, 1=dusk: falloff=2 (midpoint), 2=night: falloff=4, 3=dawn: falloff=2

DAY_R, DAY_G, DAY_B = 13, 14, 15
NIGHT_R, NIGHT_G, NIGHT_B = 2, 1, 8
MAX_FALLOFF = 4

PHASES = [
    # (name, light_falloff, sky_r, sky_g, sky_b)
    ("day",   0,                   DAY_R,   DAY_G,   DAY_B),
    ("dusk",  MAX_FALLOFF // 2,    (DAY_R + NIGHT_R) // 2, (DAY_G + NIGHT_G) // 2, (DAY_B + NIGHT_B) // 2),
    ("night", MAX_FALLOFF,         NIGHT_R, NIGHT_G, NIGHT_B),
    ("dawn",  MAX_FALLOFF // 2,    (NIGHT_R + DAY_R) // 2, (NIGHT_G + DAY_G) // 2, (NIGHT_B + DAY_B) // 2),
]

def apply_falloff(r, g, b, falloff):
    """Apply light_falloff (darkening) to (r,g,b) channels, clamping to 0."""
    r2 = max(0, r - falloff)
    g2 = max(0, g - falloff)
    b2 = max(0, b - falloff)
    return (r2, g2, b2)

# ── Color extraction from chunk_data.c ───────────────────────────────────────

def extract_colors(filepath):
    """Extract all 4-digit hex literals (potential RGB4444 values) from C source."""
    text = Path(filepath).read_text()
    # Match 0x followed by exactly 4 hex digits (case-insensitive)
    hexvals = re.findall(r'\b0x([0-9A-Fa-f]{4})\b', text)
    colors = set()
    for h in hexvals:
        v = int(h, 16)
        colors.add(v)
    return colors

# ── Median cut palette quantisation ─────────────────────────────────────────

def median_cut(colors_rgb, n_colors):
    """
    Simple median cut algorithm to reduce a set of (r,g,b) tuples to n_colors.
    Returns a list of n_colors (r,g,b) tuples (the palette).
    """
    if not colors_rgb:
        return [(0, 0, 0)] * n_colors

    # Deduplicate
    unique = list(set(colors_rgb))
    if len(unique) <= n_colors:
        # Pad with black
        while len(unique) < n_colors:
            unique.append((0, 0, 0))
        return unique[:n_colors]

    def split_bucket(bucket):
        """Split a bucket of (r,g,b) tuples along the widest channel."""
        rs = [c[0] for c in bucket]
        gs = [c[1] for c in bucket]
        bs = [c[2] for c in bucket]
        r_range = max(rs) - min(rs)
        g_range = max(gs) - min(gs)
        b_range = max(bs) - min(bs)
        if r_range >= g_range and r_range >= b_range:
            bucket.sort(key=lambda c: c[0])
        elif g_range >= b_range:
            bucket.sort(key=lambda c: c[1])
        else:
            bucket.sort(key=lambda c: c[2])
        mid = len(bucket) // 2
        return bucket[:mid], bucket[mid:]

    buckets = [unique]
    while len(buckets) < n_colors:
        # Pick the largest bucket to split
        buckets.sort(key=len, reverse=True)
        largest = buckets.pop(0)
        a, b = split_bucket(largest)
        buckets.append(a)
        buckets.append(b)

    # Average each bucket to get the palette entry
    palette = []
    for bucket in buckets:
        if not bucket:
            palette.append((0, 0, 0))
            continue
        r = sum(c[0] for c in bucket) // len(bucket)
        g = sum(c[1] for c in bucket) // len(bucket)
        b = sum(c[2] for c in bucket) // len(bucket)
        palette.append((r, g, b))

    return palette

# ── LUT generation ───────────────────────────────────────────────────────────

def build_lut(palette):
    """
    Build a 4096-entry LUT: lut[packed_rgb444_key] = nearest palette index.
    Uses squared Euclidean distance in RGB space.
    """
    lut = bytearray(4096)
    for key in range(4096):
        r, g, b = packed_rgb444_key_to_rgb(key)
        best_idx = 0
        best_dist = 10**9
        for i, (pr, pg, pb) in enumerate(palette):
            dist = (r - pr)**2 + (g - pg)**2 + (b - pb)**2
            if dist < best_dist:
                best_dist = dist
                best_idx = i
        lut[key] = best_idx
    return lut

# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent
    chunk_data_path = repo_root / "rp" / "src" / "chunk_data.c"
    output_path = repo_root / "rp" / "src" / "include" / "palette_data.h"

    if not chunk_data_path.exists():
        print(f"ERROR: {chunk_data_path} not found", file=sys.stderr)
        sys.exit(1)

    print(f"Scanning {chunk_data_path}...")
    raw_colors = extract_colors(chunk_data_path)
    print(f"  Found {len(raw_colors)} unique 4-digit hex values")

    # Convert to RGB tuples and filter out likely non-color values
    # Valid RGB4444 colors: each nibble 0-F, but we accept all since the game
    # uses values like 0x3303 (g=3, b=3, r=3) which are low-intensity
    base_colors_rgb = []
    for v in raw_colors:
        r, g, b = gbar_word_to_rgb(v)
        base_colors_rgb.append((r, g, b))

    print(f"  {len(base_colors_rgb)} base colors extracted")

    # Generate 4 palettes
    all_palettes = []
    all_luts = []

    for phase_name, falloff, sky_r, sky_g, sky_b in PHASES:
        print(f"\nPhase: {phase_name} (falloff={falloff}, sky=({sky_r},{sky_g},{sky_b}))")

        # Apply lighting to all base colors
        phase_colors = []
        for r, g, b in base_colors_rgb:
            r2, g2, b2 = apply_falloff(r, g, b, falloff)
            phase_colors.append((r2, g2, b2))

        # Add sky color
        sky_r2, sky_g2, sky_b2 = apply_falloff(sky_r, sky_g, sky_b, falloff)
        phase_colors.append((sky_r2, sky_g2, sky_b2))

        # Quantise to 16 colors
        palette = median_cut(phase_colors, 16)
        print(f"  Palette entries:")
        for i, (r, g, b) in enumerate(palette):
            st_word = rgb4444_to_st_palette_word(r, g, b)
            print(f"    [{i:2d}] RGB=({r:2d},{g:2d},{b:2d}) ST=0x{st_word:03X}")

        # Build LUT
        lut = build_lut(palette)
        all_palettes.append(palette)
        all_luts.append(lut)

    # Write header file
    print(f"\nWriting {output_path}...")
    lines = []
    lines.append("/* palette_data.h — Auto-generated by tools/gen_palette.py — do not edit */")
    lines.append("/* 4 × 16-color ST palettes + 4 × 4096-entry RGB4444→index LUTs */")
    lines.append("#ifndef PALETTE_DATA_H")
    lines.append("#define PALETTE_DATA_H")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("/* Palette names: 0=day, 1=dusk, 2=night, 3=dawn */")
    lines.append("")

    for phase_idx, (phase_name, falloff, sky_r, sky_g, sky_b) in enumerate(PHASES):
        palette = all_palettes[phase_idx]
        lut = all_luts[phase_idx]

        lines.append(f"/* {phase_name} palette (falloff={falloff}) */")
        st_words = [rgb4444_to_st_palette_word(r, g, b) for r, g, b in palette]
        words_str = ", ".join(f"0x{w:03X}" for w in st_words)
        lines.append(f"static const uint16_t palette_{phase_name}[16] = {{")
        # 8 words per line
        for i in range(0, 16, 8):
            chunk = st_words[i:i+8]
            lines.append("    " + ", ".join(f"0x{w:03X}" for w in chunk) + ",")
        lines.append("};")
        lines.append("")

        lines.append(f"/* {phase_name} LUT: maps RGB4444 key (0..4095) -> palette index */")
        lines.append(f"static const uint8_t lut_{phase_name}[4096] = {{")
        for i in range(0, 4096, 32):
            row = lut[i:i+32]
            lines.append("    " + ", ".join(str(v) for v in row) + ",")
        lines.append("};")
        lines.append("")

    lines.append("/* All 4 palettes and LUTs, indexed by daylight phase */")
    lines.append("static const uint16_t * const palettes[4] = {")
    for phase_name, _, _, _, _ in PHASES:
        lines.append(f"    palette_{phase_name},")
    lines.append("};")
    lines.append("")
    lines.append("static const uint8_t * const luts[4] = {")
    for phase_name, _, _, _, _ in PHASES:
        lines.append(f"    lut_{phase_name},")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* PALETTE_DATA_H */")

    output_path.write_text("\n".join(lines) + "\n")
    print(f"Done. Written {output_path}")

if __name__ == "__main__":
    main()
