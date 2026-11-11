#!/usr/bin/env python3
"""Install StarDict files over USB serial, with read-back verification.

Existing files with different content are preserved. Requires pyserial.
"""
import argparse
import json
import time
from pathlib import Path
import serial
from serial_link import open_device


def fnv1a(data):
    result = 2166136261
    for byte in data:
        result = ((result ^ byte) * 16777619) & 0xFFFFFFFF
    return result


def line(port, timeout=120):
    deadline = time.monotonic() + timeout
    result = bytearray()
    while time.monotonic() < deadline:
        part = port.read(1)
        if part == b"\n":
            return result.decode("utf-8", errors="replace").strip()
        result.extend(part)
    raise TimeoutError("No complete reply: " + repr(result))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port")
    parser.add_argument("prefix", type=Path, help="StarDict path without .ifo extension")
    parser.add_argument("--log", type=Path)
    args = parser.parse_args()
    port = open_device(args.port)
    results = []
    with port:
        port.reset_input_buffer()
        # USB CDC can reset on open on some hosts. Wait for the application,
        # rather than treating ROM boot output as a transfer acknowledgement.
        deadline = time.monotonic() + 60
        ready = False
        while time.monotonic() < deadline and not ready:
            port.write(b"status\n")
            try:
                while True:
                    reply = line(port, timeout=3)
                    if reply.startswith("CardputerDict "):
                        print(reply, flush=True)
                        while not line(port, timeout=3).startswith("note="):
                            pass
                        ready = True
                        break
            except TimeoutError:
                continue
        if not ready:
            raise RuntimeError("CardputerDict firmware is not ready")
        # Publish .ifo LAST so an interrupted install is not discovered as usable.
        sources = [args.prefix.with_suffix(suffix) for suffix in (".dict", ".idx")]
        licenses = args.prefix.parent.parent / "licenses"
        if licenses.is_dir():
            sources.extend(sorted(p for p in licenses.iterdir() if p.suffix in (".txt", ".json")))
        sources.append(args.prefix.with_suffix(".ifo"))
        for source in sources:
            data = source.read_bytes()
            checksum = fnv1a(data)
            port.reset_input_buffer()
            port.write(f"put {source.name} {len(data)} {checksum:08x}\n".encode("ascii"))
            reply = line(port)
            while not reply.startswith(("READY", "SKIP", "ERR")):
                reply = line(port)
            if reply.startswith("SKIP"):
                print(source.name, reply, flush=True)
                results.append({"file": source.name, "bytes": len(data), "fnv1a": f"{checksum:08x}", "result": reply})
                continue
            if reply != "READY 4096":
                raise RuntimeError(source.name + ": " + reply)
            started = time.monotonic()
            last_percent = -10
            for offset in range(0, len(data), 4096):
                chunk = data[offset:offset + 4096]
                port.write(chunk)
                expected = offset + len(chunk)
                reply = line(port, timeout=30)
                while reply.startswith("ACK ") and int(reply[4:]) <= offset:
                    reply = line(port, timeout=30)
                if reply != f"ACK {expected}":
                    raise RuntimeError(f"{source.name} at {offset}: {reply}")
                percent = expected * 100 // len(data)
                if percent >= last_percent + 10:
                    print(f"{source.name}: {percent}%", flush=True)
                    last_percent = percent
            reply = line(port, timeout=180)
            if reply != "DONE verified":
                raise RuntimeError(source.name + ": " + reply)
            result = {"file": source.name, "bytes": len(data), "fnv1a": f"{checksum:08x}", "seconds": round(time.monotonic() - started, 2), "result": reply}
            results.append(result)
            print(json.dumps(result), flush=True)
        port.write(b"reload\n")
        # Reload scans the complete index; wait for its completion log.
        deadline = time.monotonic() + 180
        while time.monotonic() < deadline:
            reply = line(port, timeout=180)
            print(reply, flush=True)
            if reply.startswith("Loaded "):
                break
        else:
            raise TimeoutError("Dictionary did not finish loading")
    if args.log:
        args.log.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
