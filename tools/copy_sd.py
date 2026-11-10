#!/usr/bin/env python3
"""Copy prepared assets to an already-mounted SD card, without formatting."""
import argparse
import hashlib
import shutil
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.digest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("volume", type=Path, help="Mounted SD card, e.g. /Volumes/CARDPUTER")
    parser.add_argument("--apply", action="store_true", help="Copy after checking all conflicts")
    args = parser.parse_args()
    volume = args.volume.resolve(strict=True)
    if volume.parent != Path("/Volumes") or not volume.is_mount():
        parser.error("Expected a mounted volume directly under /Volumes")
    source = Path(__file__).resolve().parents[1] / "sdcard"
    pending = []
    for file in sorted(source.rglob("*")):
        if not file.is_file():
            continue
        target = volume / file.relative_to(source)
        # Refuse symlinks or paths that could escape the selected volume.
        if not target.resolve().is_relative_to(volume):
            parser.error(f"Destination escapes volume: {target}")
        if target.exists():
            if not target.is_file() or digest(file) != digest(target):
                parser.error(f"Existing different file; move it aside first: {target}")
            print("Unchanged:", target)
        else:
            pending.append((file, target))
            print("Copy:", target)
    required = sum(file.stat().st_size for file, _ in pending)
    if shutil.disk_usage(volume).free < required + 1024 * 1024:
        parser.error("Insufficient free space")
    if not args.apply:
        print(f"Plan only: {len(pending)} files, {required:,} bytes. Add --apply to copy.")
        return
    for file, target in pending:
        target.parent.mkdir(parents=True, exist_ok=True)
        with file.open("rb") as src, target.open("xb") as dst:
            shutil.copyfileobj(src, dst)
        if digest(file) != digest(target):
            raise RuntimeError(f"Copy verification failed: {target}")
    print("Copy verified. Eject the volume before removing the card.")


if __name__ == "__main__":
    main()
