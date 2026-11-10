import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("prepare_dictionary", ROOT / "tools/prepare_dictionary.py")
tool = importlib.util.module_from_spec(spec)
spec.loader.exec_module(tool)


class DictionaryToolsTest(unittest.TestCase):
    def test_stardict_order_offsets_unicode(self):
        entries = {"apple": "苹果", "Apple": "苹果树", "z": "last", "字": "character"}
        with tempfile.TemporaryDirectory() as directory:
            prefix = Path(directory) / "words"
            stats = tool.write_stardict(entries, prefix, "Test")
            index = prefix.with_suffix(".idx").read_bytes()
            body = prefix.with_suffix(".dict").read_bytes()
            words, pos = [], 0
            while pos < len(index):
                end = index.index(0, pos)
                word = index[pos:end].decode("utf-8")
                offset, size = struct.unpack(">II", index[end + 1:end + 9])
                self.assertEqual(body[offset:offset + size].decode("utf-8"), entries[word])
                words.append(word)
                pos = end + 9
            self.assertEqual(words, ["Apple", "apple", "z", "字"])
            self.assertEqual(stats["words"], 4)

    def test_reject_invalid(self):
        with tempfile.TemporaryDirectory() as directory:
            for entries in ({}, {"a" * 256: "x"}, {"a\0b": "x"}, {"a": "bad\0text"}):
                with self.assertRaises(ValueError):
                    tool.write_stardict(entries, Path(directory) / "bad", "Bad")

    def test_ecdict_forms(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "words.csv"
            source.write_text('word,phonetic,translation,exchange\nrun,rʌn,"v. 跑\\nn. 奔跑",p:ran/d:run/i:running\n', encoding="utf-8")
            words = tool.load_ecdict(source)
            self.assertIn("过去式: ran", words["run"])
            self.assertIn("v. 跑\nn. 奔跑", words["run"])


if __name__ == "__main__":
    unittest.main()
