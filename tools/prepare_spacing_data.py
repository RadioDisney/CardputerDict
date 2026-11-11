#!/usr/bin/env python3
"""Generate compact Unicode wide/fullwidth and zero-spacing ranges."""
from pathlib import Path
import unicodedata


def ranges(predicate):
    result = []
    for code in range(0x110000):
        if predicate(chr(code)):
            if result and result[-1][1] + 1 == code:
                result[-1][1] = code
            else:
                result.append([code, code])
    return result


root = Path(__file__).resolve().parents[1]
wide = ranges(lambda c: unicodedata.category(c) != "Cn" and unicodedata.east_asian_width(c) in "WF")
zero = ranges(lambda c: unicodedata.category(c) in ("Mn", "Me", "Cf"))
lines = [f"// Generated from Python Unicode {unicodedata.unidata_version}; do not edit by hand.",
         "#pragma once", "#include <cstdint>", "namespace ui {", "struct Range { uint32_t first, last; };"]
for name, items in (("kWideRanges", wide), ("kZeroSpacingRanges", zero)):
    lines.append(f"inline constexpr Range {name}[] = {{")
    lines.extend(f"    {{0x{first:x}, 0x{last:x}}}," for first, last in items)
    lines.append("};")
lines.append("} // namespace ui\n")
(root / "include/unicode_spacing_data.h").write_text("\n".join(lines))
print(f"Unicode {unicodedata.unidata_version}: {len(wide)} wide ranges, {len(zero)} zero-spacing ranges")
