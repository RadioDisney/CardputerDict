#!/usr/bin/env python3
"""Build a compact single-syllable IME table from mozillazg/pinyin-data."""
import argparse
import collections
import json
import re
import unicodedata
from pathlib import Path


def normalize(syllable):
    syllable = unicodedata.normalize("NFD", syllable)
    syllable = syllable.replace("u\u0308", "v")
    return "".join(c for c in syllable if not unicodedata.combining(c))


def generate(source, target):
    groups = collections.defaultdict(set)
    for line in Path(source).read_text(encoding="utf-8").splitlines():
        match = re.match(r"U\+([0-9A-F]+): ([^#]+)", line)
        if not match:
            continue
        char = chr(int(match[1], 16))
        if not "\u4e00" <= char <= "\u9fff":
            continue
        try:
            char.encode("gb2312")
        except UnicodeEncodeError:
            continue
        for syllable in match[2].strip().split(","):
            code = normalize(syllable)
            if re.fullmatch("[a-z]{1,6}", code):
                groups[code].add(char)
    # Put common characters first; the remaining characters retain GB2312 order.
    common = "的一是在不了有和人这中大为上个国我以要他时来用们生到作地于出就分对成会可主发年动同工也能下过子说产种面而方后多定行学法所民得经十三之进着等部度家电力里如水化高自二理起小物现好你世苹果语词典书脑"
    ranks = {c: i for i, c in enumerate(dict.fromkeys(common))}
    lines = ["// Generated from mozillazg/pinyin-data (MIT); see third_party/.", "#pragma once", "struct PinyinEntry { const char* code; const char* candidates; };", "inline const PinyinEntry kPinyin[] = {"]
    for code, chars in sorted(groups.items()):
        ordered = sorted(chars, key=lambda c: (ranks.get(c, 9999), c.encode("gb2312")))
        lines.append("    {" + json.dumps(code) + ", " + json.dumps("".join(ordered), ensure_ascii=False) + "},")
    lines.extend(["};", "inline constexpr size_t kPinyinCount = sizeof(kPinyin) / sizeof(kPinyin[0]);", ""])
    Path(target).write_text("\n".join(lines), encoding="utf-8")
    print(f"Pinyin: {len(groups)} syllables, {len(set().union(*groups.values()))} characters")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    generate(args.input, args.output)
