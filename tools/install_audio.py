#!/usr/bin/env python3
"""Install a user-supplied MP3/WAV to Cardputer's SD audio/en directory.

Example: python3 tools/install_audio.py /dev/cu.usbmodemXXXX apple /path/to/apple.mp3
The file is verified on SD. Existing different files are never overwritten.
"""
import argparse
import json
from pathlib import Path
from serial_link import open_device
from device_check import command
from upload_sd import line, fnv1a
from audio_names import audio_name


def install(port, word, source):
    source = Path(source)
    name = audio_name(word, source.suffix)
    data = source.read_bytes()
    if len(data) < 32 or len(data) > 512 * 1024:
        raise ValueError("Audio must be 32..524288 bytes")
    port.reset_input_buffer()
    port.write(f"putaudio {name} {len(data)} {fnv1a(data):08x}\n".encode())
    reply = line(port)
    if reply.startswith("SKIP"):
        return {"file": name, "bytes": len(data), "result": reply}
    if reply != "READY 4096":
        raise RuntimeError(reply)
    for offset in range(0, len(data), 4096):
        chunk = data[offset:offset + 4096]; port.write(chunk)
        reply = line(port, timeout=30)
        while reply.startswith("ACK ") and int(reply[4:]) <= offset:
            reply = line(port, timeout=30)
        if reply != f"ACK {offset + len(chunk)}":
            raise RuntimeError(reply)
    reply = line(port, timeout=30)
    if reply != "DONE verified":
        raise RuntimeError(reply)
    return {"file": name, "bytes": len(data), "result": reply}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port"); parser.add_argument("word"); parser.add_argument("file", type=Path)
    args = parser.parse_args()
    with open_device(args.port, timeout=0.1) as port:
        status = command(port, "status"); assert "CardputerDict 0." in status, status
        print(json.dumps(install(port, args.word, args.file), ensure_ascii=False, indent=2))
