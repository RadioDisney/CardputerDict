from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from audio_names import audio_name


class AudioToolsTests(unittest.TestCase):
    def test_key_names(self):
        self.assertEqual(audio_name("  APPLE  butter ", ".MP3"), "apple%20butter.mp3")
        self.assertEqual(audio_name("Don't", ".wav"), "don%27t.wav")
        self.assertEqual(audio_name("U.S.", ".mp3"), "u%2es%2e.mp3")
        for word in ("苹果", "../apple", "a/b", "a?b=c", "123", "", "a" * 65):
            with self.subTest(word=word), self.assertRaises(ValueError):
                audio_name(word, ".mp3")

    def test_pack_preserves_existing_files(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp) / "source"; source.mkdir()
            target = Path(temp) / "target"
            audio = b"ID3" + b"audio fixture" * 8
            (source / "Apple butter.mp3").write_bytes(audio)
            command = [sys.executable, str(ROOT / "tools/prepare_audio_pack.py"), str(source), str(target)]
            subprocess.run(command, check=True, capture_output=True)
            stored = target / "audio/en/apple%20butter.mp3"
            self.assertEqual(stored.read_bytes(), audio)
            subprocess.run(command, check=True, capture_output=True)
            (source / "Apple butter.mp3").write_bytes(audio + b"changed")
            failed = subprocess.run(command, capture_output=True)
            self.assertNotEqual(failed.returncode, 0)
            self.assertEqual(stored.read_bytes(), audio)


if __name__ == "__main__":
    unittest.main()
