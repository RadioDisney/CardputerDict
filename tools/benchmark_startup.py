#!/usr/bin/env python3
"""Measure index creation, SD cache reloads and three actual software reboots.

Requires pyserial and Pillow. Uses existing source dictionaries on the device.
Optional --rebuild validates all three source indexes before benchmarking.
"""
import argparse
import json
import re
import time
from pathlib import Path
from serial_link import open_device
from device_check import command, screenshot


def cache_rows(text):
    return {path: {"mode": mode, "ms": int(ms), "saved": int(saved)}
            for path, mode, ms, saved in re.findall(r"cache path=(\S+) mode=(\w+) load_ms=(\d+) saved=(\d+)", text)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("--output", type=Path, default=Path("build/startup"))
    parser.add_argument("--rebuild", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    result = {}
    with open_device(args.port, timeout=0.1) as port:
        status = command(port, "status", timeout=240)
        assert "CardputerDict 0." in status, status
        result["initial_status"] = status
        print(status, flush=True)
        if args.rebuild:
            result["reindex"] = command(port, "reindex", timeout=300)
            print(result["reindex"], flush=True)
        first_ime = command(port, "ime pingguo", timeout=240)
        assert "苹果" in first_ime, first_ime
        result["first_ime"] = first_ime
        result["first_all_status"] = command(port, "status")
        print(result["first_all_status"], flush=True)
        first_caches = cache_rows(result["first_all_status"])
        assert len(first_caches) == 3
        for row in first_caches.values():
            assert row["mode"] == "cache" or row["saved"] == 1, first_caches
        started = time.monotonic()
        result["reload_output"] = command(port, "reload", timeout=240)
        result["reload_wall_ms"] = round((time.monotonic() - started) * 1000)
        assert "mode=cache" in result["reload_output"] and "mode=scan" not in result["reload_output"], result["reload_output"]
        result["cached_ime"] = command(port, "ime pingguo")
        assert "苹果" in result["cached_ime"]
        cached_status = command(port, "status")
        rows = cache_rows(cached_status)
        assert len(rows) == 3 and all(item["mode"] == "cache" for item in rows.values()), cached_status
        result["cached_status"] = cached_status
        print(cached_status, flush=True)
        result["boots"] = []
        for iteration in range(3):
            port.reset_input_buffer()
            started = time.monotonic()
            port.write(b"restart\n"); port.flush()
            deadline = time.monotonic() + 60
            logs = bytearray()
            while time.monotonic() < deadline:
                logs.extend(port.read(port.in_waiting or 1))
                if b"CardputerDict ready." in logs:
                    break
            else:
                raise RuntimeError("Restart did not finish: " + repr(logs[-500:]))
            wall_ms = round((time.monotonic() - started) * 1000)
            status = command(port, "status")
            assert "sd=1 words=770611" in status and "display=240x135 rotation=0" in status, status
            assert cache_rows(status)["/dictionaries/ecdict.ifo"]["mode"] == "cache", status
            boot_ms = int(re.search(r"boot_ready_ms=(\d+)", status)[1])
            result["boots"].append({"iteration": iteration + 1, "firmware_ms": boot_ms, "host_wall_ms": wall_ms, "status": status, "log": logs.decode("utf-8", errors="replace")})
            print(f"Cached boot {iteration + 1}: firmware={boot_ms} ms, host={wall_ms} ms", flush=True)
        assert "苹果" in command(port, "lookup apple")
        assert "/'teslә/" in command(port, "lookup TESLA")
        assert "apple" in command(port, "lookup 苹果")
        assert "苹果" in command(port, "ime pingguo")
        command(port, "press 1")
        selected = command(port, "press enter")
        assert "LOOKUP: 苹果" in selected and "apple" in selected, selected
        command(port, "lookup apple")
        screenshot(port, args.output / "dictionary.png")
        result["passed"] = True
    (args.output / "startup-benchmark.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print("Startup cache benchmark passed", flush=True)


if __name__ == "__main__":
    main()
