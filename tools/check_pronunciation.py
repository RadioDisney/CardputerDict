#!/usr/bin/env python3
"""Exercise installed pronunciation audio, cancellation and dictionary recovery.

Install apple.mp3, hello.wav first with install_audio.py. An optional --online-word
fetches a word not yet on SD, then checks that offline-only playback reuses it.
WiFi is configured on Cardputer with Fn+W; no password is read or printed here.
"""
import argparse
import json
import re
import time
from pathlib import Path
from serial_link import open_device
from device_check import command, screenshot


def wait_idle(port, timeout=45):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status = command(port, "audio status", timeout=35)
        if "state=idle" in status:
            return status
        time.sleep(0.05)
    raise TimeoutError("Pronunciation did not finish")


def check_playback(port, word, offline=True):
    first = command(port, ("speaklocal " if offline else "speak ") + word)
    if offline and "AUDIO start" not in first:
        raise RuntimeError("Local audio unavailable: " + first)
    last = wait_idle(port)
    frames = re.search(r"frames=(\d+)", last)
    peak = re.search(r"peak=(\d+)", last)
    if not frames or not peak or int(frames[1]) < 1000 or int(peak[1]) < 100:
        raise RuntimeError("No valid PCM output: " + last)
    if "wifi_connected=0" not in last:
        raise RuntimeError("WiFi was not powered off: " + last)
    return {"start": first.strip(), "done": last.strip()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--output", type=Path, default=Path("build/pronunciation"))
    parser.add_argument("--online-word")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    result = {}
    with open_device(args.port, timeout=0.1) as port:
        result["initial"] = command(port, "status")
        assert "CardputerDict 0." in result["initial"]
        result["mp3"] = check_playback(port, "apple")
        result["wav"] = check_playback(port, "hello")
        # Force all three indexes into RAM, then exercise the low-memory path.
        assert "apple" in command(port, "lookup 苹果")
        assert "苹果" in command(port, "ime pingguo")
        result["with_all_indexes"] = command(port, "status")
        result["repeated"] = []
        for i in range(5):
            result["repeated"].append(check_playback(port, "apple" if i % 2 == 0 else "hello"))
            assert "apple" in command(port, "lookup 苹果")
            assert "苹果" in command(port, "ime pingguo")
        command(port, "speaklocal apple")
        result["cancel"] = command(port, "audio stop")
        assert "state=idle" in command(port, "audio status")
        assert "苹果" in command(port, "lookup apple")
        result["invalid_word"] = command(port, "speak 苹果")
        assert "state=idle" in result["invalid_word"]
        if args.online_word:
            assert "wifi_configured=1" in command(port, "audio status")
            result["online"] = check_playback(port, args.online_word, offline=False)
            result["cached_offline"] = check_playback(port, args.online_word, offline=True)
        result["final"] = command(port, "status")
        assert "sd=1 words=770611" in result["final"]
        command(port, "lookup apple")
        screenshot(port, args.output / "ready.png")
        result["passed"] = True
    (args.output / "pronunciation-check.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
