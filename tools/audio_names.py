"""Pure-Python pronunciation filename rules shared by SD and USB tools."""
import re


def audio_name(word, suffix):
    word = " ".join(word.lower().split())
    if not re.fullmatch(r"[a-z0-9 '.-]{1,64}", word) or not re.search("[a-z]", word):
        raise ValueError("Expected an English word or short phrase")
    key = "".join(c if c.isascii() and (c.isalnum() or c == '-') else f"%{ord(c):02x}" for c in word)
    if suffix.lower() not in (".mp3", ".wav") or len(key + suffix) > 63:
        raise ValueError("Expected .mp3/.wav and an encoded filename of at most 63 bytes")
    return key + suffix.lower()
