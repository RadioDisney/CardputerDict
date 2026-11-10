#!/usr/bin/env python3
"""Convert an open DejaVuSans.ttf into the small Cardputer Latin/IPA fallback.

Requires Pillow. Extracts the original font's license into third_party/.
"""
import argparse
import hashlib
import struct
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


def generate(source, root):
    source, root = Path(source), Path(root)
    font = ImageFont.truetype(str(source), 13)
    unknown = font.getmask("\uffff")
    missing = (unknown.size, bytes(unknown))
    bitmaps, glyphs, codes = bytearray(), [], []
    for code in range(0xA0, 0x400):
        char = chr(code)
        mask = font.getmask(char)
        if (mask.size, bytes(mask)) == missing:
            continue
        x0, y0, x1, y1 = font.getbbox(char, anchor="ls")
        width, height = x1 - x0, y1 - y0
        image = Image.new("1", (max(1, width), max(1, height)))
        ImageDraw.Draw(image).text((-x0, -y0), char, font=font, anchor="ls", fill=1)
        glyphs.append((len(bitmaps), width, height, round(font.getlength(char)), x0, y0))
        codes.append(code)
        bits = [image.getpixel((x, y)) for y in range(height) for x in range(width)]
        for start in range(0, len(bits), 8):
            value = 0
            for i, bit in enumerate(bits[start:start + 8]):
                value |= bool(bit) << (7 - i)
            bitmaps.append(value)
    ranges = []
    for i, code in enumerate(codes):
        if ranges and code == ranges[-1][1] + 1:
            ranges[-1][1] = code
        else:
            ranges.append([code, code, i])
    output = ["// CardputerIpa13: generated from DejaVu Sans; license in third_party/.", "#pragma once", "#include <M5GFX.h>", "inline const uint8_t kIpaBitmaps[] PROGMEM = {"]
    for i in range(0, len(bitmaps), 20):
        output.append("    " + ",".join(f"0x{value:02x}" for value in bitmaps[i:i + 20]) + ",")
    output.append("};\ninline const lgfx::GFXglyph kIpaGlyphs[] PROGMEM = {")
    output.extend("    {" + ",".join(map(str, glyph)) + "}," for glyph in glyphs)
    output.append("};\ninline const lgfx::EncodeRange kIpaRanges[] PROGMEM = {")
    output.extend(f"    {{{a},{b},{base}}}," for a, b, base in ranges)
    output.append("};")
    output.append(f"inline const lgfx::GFXfont kIpaFont((uint8_t*)kIpaBitmaps, (lgfx::GFXglyph*)kIpaGlyphs, {codes[0]}, {codes[-1]}, 14, {len(ranges)}, (lgfx::EncodeRange*)kIpaRanges);")
    (root / "include/ipa_data.h").write_text("\n".join(output) + "\n")
    data = source.read_bytes()
    notices = []
    for i in range(struct.unpack_from(">H", data, 4)[0]):
        tag, _, offset, _ = struct.unpack_from(">4sIII", data, 12 + 16 * i)
        if tag != b"name":
            continue
        count, base = struct.unpack_from(">HH", data, offset + 2)
        for j in range(count):
            platform, _, _, nameid, size, start = struct.unpack_from(">HHHHHH", data, offset + 6 + j * 12)
            if platform == 3 and nameid in (0, 13, 14):
                notice = data[offset + base + start:offset + base + start + size].decode("utf-16-be")
                if notice not in notices:
                    notices.append(notice)
    if len(notices) < 2:
        raise ValueError("Font license metadata not found")
    (root / "third_party/DejaVuSans-LICENSE.txt").write_text("\n\n".join(notices) + "\n")
    (root / "third_party/ipa-font-source.txt").write_text(f"CardputerIpa13 bitmap subset derived from DejaVu Sans.\nSource: https://dejavu-fonts.github.io/\nTTF SHA-256: {hashlib.sha256(data).hexdigest()}\nGlyphs: {len(codes)}\n")
    print(f"IPA fallback: {len(codes)} glyphs, {len(bitmaps)} bitmap bytes")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("font", type=Path)
    parser.add_argument("--project", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    generate(args.font, args.project)
