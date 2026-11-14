#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Generate anti-aliased 2-bit-per-pixel fonts and the RGB565 palette for the
Maiskolben TFT UI.

Usage: python3 -I tools/gen_fonts.py
Writes src/fonts.h and src/colors.h.
Requires Pillow and the Inter font family (SIL Open Font License 1.1).
"""
import os
from PIL import Image, ImageDraw, ImageFont

FONT_DIR = "/usr/share/fonts/opentype/inter/"
OUT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src")

# Fixed-cell fonts: name, file, glyph strings, map chars, target digit height (px)
# A glyph may be a multi-character string (e.g. the unit "°C"); it is then
# addressed by the corresponding single map character.
FIXED_FONTS = [
    ("FONT_BIG", "InterDisplay-SemiBold.otf", list("0123456789-"), "0123456789-", 34),
    ("FONT_MED", "Inter-SemiBold.otf", list("0123456789-"), "0123456789-", 11),
    ("FONT_UNIT", "Inter-SemiBold.otf", ["°C", "°F"], "CF", 11),
]

# Proportional text font: name, file, cap height (px)
TEXT_FONTS = [
    ("FONT_TEXT", "Inter-Medium.otf", 7),
]
TEXT_EXTRA = "ÄÖÜäöüßéèêàâçñáíóúôîûÉ°·¿–▲▼"

# Tailwind / Preline slate-based palette
PALETTE = {
    "C_BG": "#020617",          # slate-950
    "C_SURFACE": "#0F172A",     # slate-900
    "C_TRACK": "#1E293B",       # slate-800
    "C_BORDER": "#334155",      # slate-700
    "C_MUTED": "#64748B",       # slate-500
    "C_SUBTLE": "#94A3B8",      # slate-400
    "C_TEXT": "#CBD5E1",        # slate-300
    "C_WHITE": "#F8FAFC",       # slate-50
    "C_ACCENT": "#2563EB",      # blue-600
    "C_ACCENT_LIGHT": "#60A5FA",  # blue-400
    "C_OK": "#22C55E",          # green-500
    "C_OK_BG": "#14532D",       # green-900
    "C_OK_FG": "#86EFAC",       # green-300
    "C_WARN": "#F59E0B",        # amber-500
    "C_WARN_BG": "#78350F",     # amber-900
    "C_WARN_FG": "#FCD34D",     # amber-300
    "C_ERR": "#EF4444",         # red-500
    "C_ERR_BG": "#7F1D1D",      # red-900
    "C_ERR_FG": "#FCA5A5",      # red-300
    "C_COOL": "#38BDF8",        # sky-400
    "C_PURE_RED": "#FF0000",    # color test
}


def rgb565(hexstr):
    r, g, b = (int(hexstr[i:i + 2], 16) for i in (1, 3, 5))
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def pack(img):
    px = list(img.tobytes())
    out = []
    for i in range(0, len(px), 4):
        b = 0
        for j in range(4):
            v = px[i + j] if i + j < len(px) else 0
            b = (b << 2) | min(3, (v + 42) // 85)
        out.append(b)
    return out


def font_for_height(path, sample, target):
    for size in range(5, 140):
        f = ImageFont.truetype(path, size)
        box = f.getbbox(sample)
        if box[3] - box[1] >= target:
            return f
    raise SystemExit("font size not found")


def fixed_font(lines, name, file, glyphs, mapchars, target):
    font = font_for_height(FONT_DIR + file, "0", target)
    top = min(font.getbbox(c)[1] for c in "0123456789")
    bottom = max(font.getbbox(c)[3] for c in "0123456789")
    height = bottom - top
    width = max(font.getbbox(c)[2] - font.getbbox(c)[0] for c in glyphs) + 1
    cname = name.lower()
    lines.append(f"// {name}: {len(glyphs)} glyphs, {width}x{height} px cell")
    lines.append(f"static const uint8_t {cname}_bits[] PROGMEM = {{")
    for ch in glyphs:
        img = Image.new("L", (width, height), 0)
        box = font.getbbox(ch)
        x = (width - (box[2] - box[0])) // 2 - box[0]
        ImageDraw.Draw(img).text((x, -top), ch, font=font, fill=255)
        data = pack(img)
        lines.append("  // " + " ".join(f"U+{ord(x):04X}" for x in ch))
        for i in range(0, len(data), 16):
            lines.append("  " + ", ".join(f"0x{b:02X}" for b in data[i:i + 16]) + ",")
    lines.append("};")
    lines.append(f'static const char {cname}_map[] PROGMEM = "{mapchars}";')
    lines.append(f"static const FixedFont {name} PROGMEM = "
                 f"{{ {cname}_bits, {cname}_map, {width}, {height}, {len(glyphs)} }};\n")
    return width, height


def text_font(lines, name, file, cap):
    font = font_for_height(FONT_DIR + file, "H", cap)
    ascent, descent = font.getmetrics()
    chars = [chr(c) for c in range(32, 127)] + sorted(TEXT_EXTRA, key=ord)
    # Common vertical window: tallest accent to deepest descender.
    top = min(font.getbbox(c)[1] for c in chars if c.strip())
    bottom = max(font.getbbox(c)[3] for c in chars if c.strip())
    height = bottom - top
    baseline = -top + ascent  # informational only
    cname = name.lower()
    bits, offsets, widths, cps = [], [], [], []
    for ch in chars:
        adv = max(1, round(font.getlength(ch)))
        img = Image.new("L", (adv, height), 0)
        ImageDraw.Draw(img).text((0, -top), ch, font=font, fill=255)
        offsets.append(len(bits))
        widths.append(adv)
        cps.append(ord(ch))
        bits.extend(pack(img))
    lines.append(f"// {name}: {len(chars)} glyphs, height {height} px, {len(bits)} bytes")
    lines.append(f"static const uint8_t {cname}_bits[] PROGMEM = {{")
    for i in range(0, len(bits), 16):
        lines.append("  " + ", ".join(f"0x{b:02X}" for b in bits[i:i + 16]) + ",")
    lines.append("};")
    lines.append(f"static const uint16_t {cname}_offsets[] PROGMEM = {{ "
                 + ", ".join(str(o) for o in offsets) + " };")
    lines.append(f"static const uint8_t {cname}_widths[] PROGMEM = {{ "
                 + ", ".join(str(w) for w in widths) + " };")
    extra = cps[95:]
    lines.append(f"static const uint16_t {cname}_extra[] PROGMEM = {{ "
                 + ", ".join(f"0x{c:04X}" for c in extra) + " };")
    lines.append(f"static const TextFont {name} PROGMEM = {{ {cname}_bits, {cname}_offsets, "
                 f"{cname}_widths, {cname}_extra, {height}, {len(extra)} }};\n")
    return height, len(bits), baseline


def main():
    lines = [
        "// Generated by tools/gen_fonts.py - do not edit by hand.",
        "// Glyph shapes derived from Inter (SIL Open Font License 1.1).",
        "#pragma once",
        "#include <Arduino.h>",
        '#include "fontdefs.h"',
        "",
    ]
    for args in FIXED_FONTS:
        w, h = fixed_font(lines, *args)
        print(f"{args[0]}: cell {w}x{h}")
    for args in TEXT_FONTS:
        h, size, base = text_font(lines, *args)
        print(f"{args[0]}: height {h}, {size} bytes")
    with open(os.path.join(OUT_DIR, "fonts.h"), "w") as f:
        f.write("\n".join(lines) + "\n")

    col = [
        "// Generated by tools/gen_fonts.py - do not edit by hand.",
        "// RGB565 palette based on the Tailwind CSS slate scale (Preline look).",
        "#pragma once",
        "",
    ]
    for k, v in PALETTE.items():
        col.append(f"#define {k:<16} 0x{rgb565(v):04X}  // {v}")
    with open(os.path.join(OUT_DIR, "colors.h"), "w") as f:
        f.write("\n".join(col) + "\n")


if __name__ == "__main__":
    main()
