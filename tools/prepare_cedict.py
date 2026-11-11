#!/usr/bin/env python3
"""Convert CC-CEDICT into Chinese-English StarDict and a phrase Pinyin index.

Derived dictionary data is CC BY-SA 4.0; application source is separately MIT.
"""
import argparse
import collections
import gzip
import hashlib
import json
import re
from pathlib import Path
from prepare_dictionary import write_stardict

ENTRY = re.compile(r"^(\S+) (\S+) \[([^]]+)\] /(.+)/$")


def normalize_pinyin(text):
    return re.sub(r"[1-5\s']", "", text.lower().replace("u:", "v").replace("ü", "v"))


def records(source):
    opener = gzip.open if str(source).endswith(".gz") else open
    with opener(source, "rt", encoding="utf-8") as stream:
        for number, line in enumerate(stream, 1):
            if not line.strip() or line.startswith("#"):
                continue
            match = ENTRY.match(line.rstrip())
            if not match:
                raise ValueError(f"Invalid CC-CEDICT entry at line {number}")
            yield match.groups()


def convert(source, output):
    meanings = collections.defaultdict(list)
    pinyin = collections.defaultdict(dict)
    entries = 0
    common = "的一是在不了有和人这中大为上个国我以要他时来用们生到作地于出就分对成会可主发年动同工也能下过子说面方后多定行学法民得经十三进着部家电力里如水高自二理小现好你世苹果语词典书脑平果绿女"
    priority = {c: i for i, c in enumerate(dict.fromkeys(common))}
    for traditional, simplified, pronunciation, gloss in records(source):
        entries += 1
        meaning = f"[{pronunciation}]\n" + "\n".join(gloss.split("/"))
        for word in dict.fromkeys((simplified, traditional)):
            if len(word.encode("utf-8")) <= 255 and meaning not in meanings[word]:
                meanings[word].append(meaning)
        code = normalize_pinyin(pronunciation)
        if not re.fullmatch("[a-z]{1,32}", code) or not re.fullmatch(r"[\u4e00-\u9fff]{1,16}", simplified):
            continue
        # Limit IME choices to the GB2312 characters covered by the base font.
        try:
            simplified.encode("gb2312")
        except UnicodeEncodeError:
            continue
        uncommon = bool(re.search(r"\b(surname|variant of|old variant|archaic|obsolete)\b", gloss, re.I))
        score = (uncommon, sum(priority.get(c, 500) for c in simplified), len(simplified), simplified)
        old = pinyin[code].get(simplified)
        if old is None or score < old:
            pinyin[code][simplified] = score
    lexical = {word: "\n\n".join(parts) for word, parts in meanings.items()}
    candidates = {code: "\n".join(sorted(words, key=words.get)[:192]) for code, words in pinyin.items()}
    output = Path(output)
    stats = {
        "source_entries": entries,
        "chinese_english": write_stardict(lexical, output / "cedict", "CC-CEDICT 汉英"),
        "pinyin": write_stardict(candidates, output / "pinyin", "CC-CEDICT Pinyin IME"),
        "source_sha256": hashlib.sha256(Path(source).read_bytes()).hexdigest(),
        "license": "CC BY-SA 4.0",
    }
    # Validate useful cases before publishing generated data.
    assert "apple" in lexical["苹果"] and "苹果" in candidates["pingguo"].splitlines()
    assert "你好" in candidates["nihao"].splitlines()
    return stats


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--stats", type=Path)
    args = parser.parse_args()
    result = convert(args.source, args.output)
    if args.stats:
        args.stats.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
