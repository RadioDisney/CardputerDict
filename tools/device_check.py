#!/usr/bin/env python3
"""Check the running firmware over USB serial; optionally capture its framebuffer.

Requires pyserial and Pillow on the host. Does not flash or reset the device.
"""
import argparse
import io
import json
import re
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


def command(port, text, timeout=120):
    port.reset_input_buffer()
    port.write((text + "\n").encode("utf-8"))
    port.flush()
    result = bytearray()
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result.extend(port.read(port.in_waiting or 1))
        if b"CMD_DONE\r\n" in result or b"CMD_DONE\n" in result:
            return result.decode("utf-8", errors="replace")
    raise TimeoutError("Command did not complete: " + text + " " + repr(result[-200:]))


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
    parser.add_argument("--bilingual", action="store_true")
    parser.add_argument("--compact", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    port = open_device(args.port, timeout=0.1)
    with port:
        read_until_idle(port, timeout=2)
        status = ""
        deadline = time.monotonic() + 120
        while time.monotonic() < deadline:
            status = command(port, "status")
            if "CardputerDict 0.2." in status:
                status = command(port, "status")
                break
        if "CardputerDict 0.2." not in status:
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
        if args.bilingual:
            chinese = command(port, "lookup 苹果")
            if "apple" not in chinese or "LOOKUP: 苹果" not in chinese:
                raise RuntimeError("Chinese-English lookup failed: " + chinese)
            screenshot(port, args.output / "chinese-definition.png")
            traditional = command(port, "lookup 蘋果")
            if "apple" not in traditional:
                raise RuntimeError("Traditional lookup failed: " + traditional)
            command(port, "lookup apple")
            command(port, "press tab")
            typed = command(port, "type pingguo")
            if "composition=pingguo" not in typed or "direction=zh-en" not in typed:
                raise RuntimeError("Typing full Pinyin failed: " + typed)
            screenshot(port, args.output / "phrase-pinyin.png")
            selected = command(port, "press 1")
            if "query=苹果 composition= " not in selected:
                raise RuntimeError("Whole-word selection failed: " + selected)
            opened = command(port, "press enter")
            if "LOOKUP: 苹果" not in opened or "apple" not in opened:
                raise RuntimeError("IME-to-dictionary flow failed: " + opened)
            command(port, "press clear")
            command(port, "type ping")
            paged = command(port, "press ]")
            if "candidate_page=1" not in paged:
                raise RuntimeError("Candidate pagination failed: " + paged)
            screenshot(port, args.output / "candidate-page.png")
            command(port, "press [")
            command(port, "press clear")
            command(port, "type nihao")
            hello = command(port, "press space")
            if "query=你好 composition= " not in hello:
                raise RuntimeError("Phrase space selection failed: " + hello)
            deleted = command(port, "press backspace")
            if "query=你 composition= " not in deleted:
                raise RuntimeError("UTF-8 backspace failed: " + deleted)
            command(port, "press clear")
            command(port, "type lve")
            umlaut = command(port, "press 1")
            if "query=略 composition= " not in umlaut and "query=掠 composition= " not in umlaut:
                raise RuntimeError("v for umlaut failed: " + umlaut)
            command(port, "lookup apple")
            started = time.monotonic()
            direction = command(port, "press tab")
            elapsed = time.monotonic() - started
            if "direction=zh-en" not in direction or "Loaded " in direction or elapsed > 2:
                raise RuntimeError("Cached direction switch failed: " + direction)
            final_status = command(port, "status")
            if "words=198650 " not in final_status or "ime=85610" not in final_status or "display=240x135 rotation=0" not in final_status:
                raise RuntimeError("Final bilingual state failed: " + final_status)
            command(port, "lookup apple")
            result.update(chinese=chinese.strip(), traditional=traditional.strip(), typed=typed.strip(), selected=selected.strip(), opened=opened.strip(),
                          hello=hello.strip(), deleted=deleted.strip(), umlaut=umlaut.strip(), switch_seconds=round(elapsed, 3), bilingual_status=final_status.strip())
        if args.compact:
            def candidate_words(response):
                return next(line for line in response.splitlines() if line.startswith("CANDIDATES:")).split()[1:]

            def value(response, name):
                match = re.search(r"\b" + name + r"=(\d+)", response)
                if not match:
                    raise RuntimeError("Missing input state: " + response)
                return int(match.group(1))

            response = command(port, "ime ping")
            single_words = candidate_words(response)
            single_state = command(port, "press [")
            if value(single_state, "visible") != 8 or value(single_state, "candidate_start") != 0:
                raise RuntimeError("Single-character packing failed: " + single_state)
            screenshot(port, args.output / "compact-single.png")
            hidden = command(port, "press 9")
            if "query= composition=ping " not in hidden:
                raise RuntimeError("Invisible candidate was selected: " + hidden)
            eighth = command(port, "press 8")
            if "query=" + single_words[7] + " composition= " not in eighth:
                raise RuntimeError("Eighth candidate mapping failed: " + eighth)
            command(port, "ime ping")
            second_state = command(port, "press ]")
            if value(second_state, "candidate_start") != 8:
                raise RuntimeError("Variable-page boundary failed: " + second_state)
            screenshot(port, args.output / "compact-page-two.png")
            next_selected = command(port, "press 1")
            if "query=" + single_words[8] + " composition= " not in next_selected:
                raise RuntimeError("Second-page candidate mapping failed: " + next_selected)
            response = command(port, "ime shishi")
            phrase_words = candidate_words(response)
            phrase_state = command(port, "press [")
            if value(phrase_state, "visible") != 5:
                raise RuntimeError("Two-character packing failed: " + phrase_state)
            screenshot(port, args.output / "compact-phrases.png")
            fifth = command(port, "press 5")
            if "query=" + phrase_words[4] + " composition= " not in fifth:
                raise RuntimeError("Whole-phrase selection failed: " + fifth)
            command(port, "lookup apple")
            result.update(compact_single=single_state.strip(), compact_hidden=hidden.strip(), compact_eighth=eighth.strip(),
                          compact_page_two=second_state.strip(), compact_next=next_selected.strip(), compact_phrases=phrase_state.strip(), compact_fifth=fifth.strip())
        (args.output / "serial-check.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
