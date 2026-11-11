#pragma once
#include <algorithm>
#include <string>
#include "unicode_spacing_data.h"

namespace ui {
template <size_t N>
inline bool inRanges(uint32_t code, const Range (&ranges)[N]) {
    size_t lo = 0, hi = N;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (code < ranges[mid].first) hi = mid;
        else if (code > ranges[mid].last) lo = mid + 1;
        else return true;
    }
    return false;
}

inline int letterSpacing(uint32_t code) {
    if (code < 32 || code == 127 || inRanges(code, kZeroSpacingRanges)) return 0;
    return inRanges(code, kWideRanges) ? 2 : 1;
}

struct Character { uint32_t code; size_t bytes; };
inline Character decode(const std::string& text, size_t pos) {
    const auto first = uint8_t(text[pos]);
    const size_t bytes = first < 0x80 ? 1 : (first & 0xe0) == 0xc0 ? 2 :
                         (first & 0xf0) == 0xe0 ? 3 : (first & 0xf8) == 0xf0 ? 4 : 0;
    if (!bytes || bytes > text.size() - pos) return {0xfffd, 1};
    uint32_t code = first & (bytes == 1 ? 0x7f : bytes == 2 ? 0x1f : bytes == 3 ? 0x0f : 0x07);
    for (size_t i = 1; i < bytes; ++i) {
        if ((uint8_t(text[pos + i]) & 0xc0) != 0x80) return {0xfffd, 1};
        code = (code << 6) | (uint8_t(text[pos + i]) & 0x3f);
    }
    if ((bytes == 2 && code < 0x80) || (bytes == 3 && code < 0x800) ||
        (bytes == 4 && code < 0x10000) || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff)) return {0xfffd, 1};
    return {code, bytes};
}

inline int extraWidth(const std::string& text) {
    int extra = 0, previousSpacing = 0;
    bool havePrevious = false;
    for (size_t pos = 0; pos < text.size();) {
        const auto character = decode(text, pos);
        const int spacing = letterSpacing(character.code);
        if (spacing) {
            if (havePrevious) extra += previousSpacing;
            previousSpacing = spacing; havePrevious = true;
        }
        pos += character.bytes;
    }
    return extra;
}

template <typename Surface>
int textWidth(Surface& surface, const std::string& text) {
    const size_t newline = text.find('\n');
    if (newline != std::string::npos)
        return std::max(textWidth(surface, text.substr(0, newline)), textWidth(surface, text.substr(newline + 1)));
    return surface.textWidth(text.c_str()) + extraWidth(text);
}

template <typename Surface>
void printText(Surface& surface, const std::string& text, int x, int y) {
    surface.setCursor(x, y);
    int previousSpacing = 0;
    bool havePrevious = false;
    for (size_t pos = 0; pos < text.size();) {
        const auto character = decode(text, pos);
        if (character.code == '\r') { pos += character.bytes; continue; }
        if (character.code == '\n') {
            surface.print("\n"); y = surface.getCursorY(); surface.setCursor(x, y);
            previousSpacing = 0; havePrevious = false; pos += character.bytes; continue;
        }
        const int spacing = letterSpacing(character.code);
        if (spacing && havePrevious) surface.setCursor(surface.getCursorX() + previousSpacing, y);
        surface.print(text.substr(pos, character.bytes).c_str());
        if (spacing) { previousSpacing = spacing; havePrevious = true; }
        pos += character.bytes;
    }
}
} // namespace ui
