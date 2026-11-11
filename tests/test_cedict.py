import gzip
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from prepare_cedict import convert, normalize_pinyin, records


def read_dictionary(prefix):
    index = prefix.with_suffix(".idx").read_bytes()
    body = prefix.with_suffix(".dict").read_bytes()
    result = {}
    offset = 0
    while offset < len(index):
        end = index.index(0, offset)
        start, length = struct.unpack(">II", index[end + 1:end + 9])
        result[index[offset:end].decode()] = body[start:start + length].decode()
        offset = end + 9
    return result


class CedictTests(unittest.TestCase):
    def test_normalization(self):
        self.assertEqual(normalize_pinyin("Ping2 guo3"), "pingguo")
        self.assertEqual(normalize_pinyin("lu:4"), "lv")
        self.assertEqual(normalize_pinyin("lü4"), "lv")
        self.assertEqual(normalize_pinyin("Xi1 an1"), "xian")

    def test_polyphony_variants_and_phrase_candidates(self):
        content = "\n".join([
            "# CC-CEDICT fixture",
            "蘋果 苹果 [ping2 guo3] /apple/",
            "你好 你好 [ni3 hao3] /hello/hi/",
            "行 行 [xing2] /to walk/",
            "行 行 [hang2] /row/profession/",
            "綠 绿 [lu:4] /green/",
            "女 女 [nu:3] /woman/",
        ]) + "\n"
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "cedict.gz"
            with gzip.open(source, "wt", encoding="utf-8") as stream:
                stream.write(content)
            stats = convert(source, Path(temp) / "dictionaries")
            lexical = read_dictionary(Path(temp) / "dictionaries/cedict")
            pinyin = read_dictionary(Path(temp) / "dictionaries/pinyin")
            self.assertEqual(lexical["苹果"], lexical["蘋果"])
            self.assertIn("apple", lexical["苹果"])
            self.assertIn("to walk", lexical["行"])
            self.assertIn("profession", lexical["行"])
            self.assertEqual(pinyin["pingguo"].splitlines(), ["苹果"])
            self.assertIn("行", pinyin["xing"].splitlines())
            self.assertIn("行", pinyin["hang"].splitlines())
            self.assertIn("绿", pinyin["lv"].splitlines())
            self.assertEqual(stats["source_entries"], 6)

    def test_invalid_line_is_not_silently_skipped(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "invalid.txt"
            source.write_text("invalid cedict entry\n")
            with self.assertRaises(ValueError):
                list(records(source))


if __name__ == "__main__":
    unittest.main()
