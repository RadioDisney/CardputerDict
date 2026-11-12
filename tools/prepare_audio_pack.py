#!/usr/bin/env python3
"""Prepare a user-owned MP3/WAV collection for offline pronunciation.

Input files are named after English headwords, e.g. Apple.mp3, apple butter.wav.
No network download is performed. Each file is stored using the firmware's
normalized UTF-8-safe name under audio/en/. Existing differing files are refused.
"""
import argparse
import hashlib
import json
from pathlib import Path
from audio_names import audio_name


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="Directory of MP3/WAV files")
    parser.add_argument("output", type=Path, help="SD root or staging directory")
    args = parser.parse_args()
    target = args.output / "audio/en"
    target.mkdir(parents=True, exist_ok=True)
    entries = []
    for source in sorted(args.source.glob("*")):
        if not source.is_file() or source.suffix.lower() not in (".mp3", ".wav"):
            continue
        name = audio_name(source.stem, source.suffix)
        data = source.read_bytes()
        if len(data) < 32 or len(data) > 512 * 1024:
            raise ValueError(f"Audio file must be 32..524288 bytes: {source.name}")
        destination = target / name
        if destination.exists() and destination.read_bytes() != data:
            raise FileExistsError(f"Existing different audio preserved: {destination}")
        if not destination.exists():
            with destination.open("xb") as output:
                output.write(data)
        entries.append({"word": source.stem, "file": "audio/en/" + name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()})
    (args.output / "audio-pack.json").write_text(json.dumps(entries, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"Prepared {len(entries)} pronunciation files. Copy audio/ to your FAT32 SD root.")


if __name__ == "__main__":
    main()
