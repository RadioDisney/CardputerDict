"""Verify coverage and metrics against the pinned M5GFX font payloads."""
import ast
import functools
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / '.pio/libdeps/cardputer/M5GFX/src/lgfx/Fonts/efont'

@functools.lru_cache(None)
def font_bytes(region, size):
    source = (FONTS / f'lgfx_efont_{region}.c').read_text()
    match = re.search(rf'lgfx_efont_{region}_{size}\[(\d+)\]\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);', source, re.S)
    assert match, (region, size)
    data = b''.join(ast.literal_eval('b' + part) for part in re.findall(r'"(?:[^"\\]|\\.)*"', match[2])) + b'\0'
    assert len(data) == int(match[1])
    return data

def glyph_metrics(font, character):
    code = ord(character)
    assert code > 255
    at = lookup = 23 + (font[21] << 8 | font[22])
    while True:
        at += font[lookup] << 8 | font[lookup + 1]
        end = font[lookup + 2] << 8 | font[lookup + 3]
        lookup += 4
        if end >= code:
            break
    while (font[at] << 8 | font[at + 1]) != 0:
        if (font[at] << 8 | font[at + 1]) == code:
            bits = int.from_bytes(font[at + 3:at + 11], 'little')
            values = []
            for count in font[4:9]:
                values.append(bits & ((1 << count) - 1))
                bits >>= count
            return values[0], values[1], values[4] - (1 << (font[8] - 1))
        at += font[at + 2]
    return None

@unittest.skipUnless(FONTS.is_dir(), 'Install pinned PlatformIO dependencies to inspect actual font data')
class CjkCoverageTests(unittest.TestCase):
    def test_missing_traditional_glyphs_have_real_fallback(self):
        for size in (12, 14, 16):
            cn, tw = font_bytes('cn', size), font_bytes('tw', size)
            for char in '個顆蘋':
                with self.subTest(size=size, char=char):
                    self.assertIsNone(glyph_metrics(cn, char))
                    width, height, advance = glyph_metrics(tw, char)
                    self.assertGreater(width, 0)
                    self.assertGreater(height, 0)
                    self.assertLessEqual(height, size)
                    self.assertEqual(advance, size)

    def test_simplified_and_mixed_words_remain_covered(self):
        for size in (12, 14, 16):
            cn, tw = font_bytes('cn', size), font_bytes('tw', size)
            for char in '个颗苹果蘋果個顆漢英詞典':
                with self.subTest(size=size, char=char):
                    self.assertIsNotNone(glyph_metrics(cn, char) or glyph_metrics(tw, char))
            for char in '个颗苹果':
                self.assertIsNotNone(glyph_metrics(cn, char))

    def test_fallback_uses_same_line_height(self):
        for size in (12, 14, 16):
            cn, tw = font_bytes('cn', size), font_bytes('tw', size)
            self.assertEqual(cn[10], size)
            self.assertEqual(tw[10], size)
            self.assertEqual(cn[12], tw[12])
