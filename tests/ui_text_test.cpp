#include "ui_text.h"
#include "ime_core.h"
#include <cassert>
#include <iostream>
#include <vector>

struct Surface {
    struct Glyph { uint32_t code; int x, y; };
    std::vector<Glyph> glyphs;
    int x = 0, y = 0, half = 7, full = 14;
    int advance(uint32_t c) const {
        if (c < 32 || c == 0x0301 || c == 0xfe0f) return 0;
        return (c == 0x4e2d || c == 0x82f9 || c == 0x679c || c == 0xff21 || c == 0x3000 || c == 0xff0c) ? full : half;
    }
    int textWidth(const char* value) const {
        const std::string text(value);
        int width = 0;
        for (size_t pos = 0; pos < text.size();) {
            const auto c = ui::decode(text, pos);
            width += advance(c.code); pos += c.bytes;
        }
        return width;
    }
    void setCursor(int a, int b) { x = a; y = b; }
    int getCursorX() const { return x; }
    int getCursorY() const { return y; }
    void print(const char* value) {
        const std::string text(value);
        for (size_t pos = 0; pos < text.size();) {
            const auto c = ui::decode(text, pos); pos += c.bytes;
            if (c.code == '\n') { x = 0; y += 14; continue; }
            glyphs.push_back({c.code, x, y}); x += advance(c.code);
        }
    }
};

int main() {
    for (uint32_t c : {uint32_t('A'), uint32_t('7'), uint32_t(' '), uint32_t(','), 0xe6u, 0x259u, 0xff71u})
        assert(ui::letterSpacing(c) == 1);
    for (uint32_t c : {0x4e2du, 0x82f9u, 0xff21u, 0xff0cu, 0x3000u, 0x3002u, 0xac00u, 0x1f600u, 0x20000u})
        assert(ui::letterSpacing(c) == 2);
    for (uint32_t c : {0x301u, 0xfe0fu, 0x200du, uint32_t('\n')}) assert(ui::letterSpacing(c) == 0);
    assert(ui::extraWidth("A中B") == 3);
    assert(ui::extraWidth("中A") == 2 && ui::extraWidth("A中") == 1);
    assert(ui::extraWidth("Ａｱ") == 2);
    assert(ui::extraWidth("a\u0301b") == 1);
    assert(ui::extraWidth("") == 0 && ui::extraWidth("中") == 0);
    assert(ui::decode("\xf0\x9f\x98\x80", 0).code == 0x1f600);
    assert(ui::decode("\xe4", 0).code == 0xfffd);
    assert(ui::decode("\xc0\xaf", 0).code == 0xfffd);

    Surface mixed;
    ui::printText(mixed, "A中B", 0, 5);
    assert(mixed.glyphs.size() == 3);
    assert(mixed.glyphs[0].x == 0 && mixed.glyphs[1].x == 8 && mixed.glyphs[2].x == 24);
    assert(mixed.x == 31 && ui::textWidth(mixed, "A中B") == 31);
    Surface combined;
    ui::printText(combined, "a\u0301b", 4, 7);
    assert(combined.glyphs[1].x == 11 && combined.glyphs[2].x == 12);
    assert(combined.x == 4 + ui::textWidth(combined, "a\u0301b"));
    Surface multiline;
    ui::printText(multiline, "AB\n中A", 4, 7);
    assert(multiline.glyphs[2].x == 4 && multiline.glyphs[2].y == 21);
    assert(multiline.glyphs[3].x == 20 && ui::textWidth(multiline, "AB\n中A") == 23);

    Surface candidate;
    candidate.half = 8; candidate.full = 16;
    ime::State state;
    state.candidates.assign(12, "苹果");
    state.reflow([&](const std::string& text) { return ui::textWidth(candidate, text); });
    assert(state.visibleCount() == 5 && state.placements[0].wordWidth == 34);
    assert(state.placements[1].x == 47 && state.pageCount() == 3);
    state.turnPage(1);
    std::string query;
    assert(state.pageStart() == 5 && state.select(4, query) && query == "苹果");
    std::cout << "Global typography passed: half/full widths, punctuation, combining marks, exact glyph positions, multiline and candidate pagination.\n";
}
