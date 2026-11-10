#!/usr/bin/env python3
"""Check the running firmware over USB serial; optionally capture its framebuffer.

Requires pyserial and Pillow on the host. Does not flash or reset the device.
"""
import argparse
import io
import json
import time
from pathlib import Path

import serial
from PIL import Image
from serial_link import open_device


def read_until_idle(port, timeout=4, idle=0.3):
    result = bytearray()
    deadline = time.monotonic() + timeout
    last = time.monotonic()
    while time.monotonic() < deadline:
        chunk = port.read(port.in_waiting or 1)
        if chunk:
            result.extend(chunk)
            last = time.monotonic()
        elif result and time.monotonic() - last > idle:
            break
    return bytes(result)


def command(port, text):
    port.reset_input_buffer()
    port.write((text + "\n").encode("utf-8"))
    port.flush()
    return read_until_idle(port).decode("utf-8", errors="replace")


def screenshot(port, target):
    port.reset_input_buffer()
    port.write(b"screenshot\n")
    deadline = time.monotonic() + 8
    prefix = bytearray()
    marker = b"PPM_BEGIN\nP6\n240 135\n255\n"
    while not prefix.endswith(marker):
        if time.monotonic() > deadline:
            raise TimeoutError("Screenshot header not received: " + repr(prefix[-200:]))
        prefix.extend(port.read(1))
    pixels = bytearray()
    length = 240 * 135 * 3
    while len(pixels) < length:
        if time.monotonic() > deadline:
            raise TimeoutError(f"Partial screenshot: {len(pixels)}/{length}")
        pixels.extend(port.read(length - len(pixels)))
    image = Image.open(io.BytesIO(b"P6\n240 135\n255\n" + pixels))
    image.save(target)
    read_until_idle(port, timeout=1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--output", type=Path, default=Path("build/device"))
    parser.add_argument("--expected-words", type=int)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    port = open_device(args.port, timeout=0.1)
    with port:
        read_until_idle(port, timeout=2)
        status = ""
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            status = command(port, "status")
            if "CardputerDict 0.1.0" in status:
                status = command(port, "status")
                break
        if "CardputerDict 0.1.0" not in status:
            raise RuntimeError("Firmware not responding: " + status)
        if args.expected_words is not None and f"words={args.expected_words} " not in status:
            raise RuntimeError("Unexpected dictionary wordcount: " + status)
        missing = command(port, "lookup zzzzzzzznotaword")
        if "NOT_FOUND" not in missing:
            raise RuntimeError("Missing-word check failed: " + missing)
        apple = command(port, "lookup apple")
        if "苹果" not in apple:
            raise RuntimeError("Chinese definition check failed: " + apple)
        screenshot(port, args.output / "definition.png")
        pinyin = command(port, "ime ping")
        if "苹" not in pinyin:
            raise RuntimeError("Pinyin candidate check failed: " + pinyin)
        screenshot(port, args.output / "pinyin.png")
        command(port, "lookup apple")
        result = {"status": status.strip(), "apple": apple.strip(), "missing": missing.strip(), "pinyin": pinyin.strip(), "passed": True}
        (args.output / "serial-check.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
