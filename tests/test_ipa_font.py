import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class PhoneticFontTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = (ROOT / "include/ipa_data.h").read_text()
        bitmap = source.split("kIpaBitmaps[]", 1)[1].split("};", 1)[0]
        cls.bitmap = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]+)", bitmap))
        def records(name):
            text = source.split(name + "[]", 1)[1].split("};", 1)[0]
            return [tuple(map(int, re.findall(r"-?\d+", row))) for row in re.findall(r"\{([^{}]+)\}", text)]
        cls.glyphs = records("kIpaGlyphs")
        cls.ranges = records("kIpaRanges")

    def glyph(self, character):
        code = ord(character)
        for first, last, base in self.ranges:
            if first <= code <= last:
                return self.glyphs[base + code - first]
        return None

    def test_tesla_cyrillic_schwa_is_visible(self):
        pronunciation = "/'teslә/"
        self.assertEqual(ord(pronunciation[-2]), 0x04D9)
        glyph = self.glyph(pronunciation[-2])
        self.assertIsNotNone(glyph)
        offset, width, height, advance, _, _ = glyph
        self.assertGreater(width, 0)
        self.assertGreater(height, 0)
        self.assertGreater(advance, 0)
        self.assertTrue(any(self.bitmap[offset:offset + (width * height + 7) // 8]))

    def test_existing_ipa_remains_available(self):
        for character in "ɑɔəәɛɜɪʊʌʃʒŋθðæˈˌ":
            with self.subTest(character=character):
                self.assertIsNotNone(self.glyph(character))
        self.assertIsNone(self.glyph("中"))

    def test_generated_ranges_and_bitmap_bounds(self):
        next_glyph = 0
        previous_last = -1
        for first, last, base in self.ranges:
            self.assertGreater(first, previous_last)
            self.assertLessEqual(first, last)
            self.assertEqual(base, next_glyph)
            next_glyph += last - first + 1
            previous_last = last
        self.assertEqual(next_glyph, len(self.glyphs))
        for offset, width, height, _, _, _ in self.glyphs:
            self.assertLessEqual(offset + (width * height + 7) // 8, len(self.bitmap))


if __name__ == "__main__":
    unittest.main()
