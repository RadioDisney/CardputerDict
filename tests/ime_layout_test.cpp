#include "ime_core.h"
#include <cassert>
#include <iostream>

int width(const std::string& text) {
    int result = 0;
    for (size_t i = 0; i < text.size(); i += dict::utf8Length(uint8_t(text[i])))
        result += uint8_t(text[i]) < 128 ? 8 : 16;
    return result;
}

void checkGeometry(ime::State state, int lineWidth = 232) {
    size_t next = 0;
    for (size_t page = 0; page < state.pageCount(); ++page) {
        state.page = int(page);
        assert(state.pageStart() == next);
        assert(state.visibleCount() <= 9);
        int previousEnd = -4;
        for (size_t item = 0; item < state.visibleCount(); ++item) {
            const size_t index = state.pageStart() + item;
            const auto& place = state.placements[index];
            assert(place.x >= previousEnd + 4);
            previousEnd = place.x + place.labelWidth + 1 + place.wordWidth;
            assert(previousEnd <= lineWidth);
            auto selected = state;
            std::string query;
            assert(selected.select(int(item), query));
            assert(query == state.candidates[index]);
            ++next;
        }
        auto invalid = state;
        std::string query;
        assert(!invalid.select(int(state.visibleCount()), query));
        assert(query.empty() && invalid.input == state.input);
    }
    assert(next == state.candidates.size());
}

int main() {
    ime::State single;
    const std::string chars = "苹平乒俜凭坪娉屏枰瓶萍评鲆冯秤砰聘堋";
    for (size_t i = 0; i < chars.size(); i += 3) single.candidates.push_back(chars.substr(i, 3));
    single.input = "ping";
    single.reflow(width);
    assert(single.pageCount() == 3 && single.visibleCount() == 8);
    single.turnPage(1);
    assert(single.pageStart() == 8 && single.visibleCount() == 8);
    single.turnPage(100);
    assert(single.pageStart() == 16 && single.visibleCount() == 2);
    single.turnPage(-100); assert(single.page == 0);
    checkGeometry(single);

    ime::State phrases;
    phrases.candidates = {"苹果", "平果", "平锅", "你好", "世界", "中国", "学习", "工作", "词典", "屏幕"};
    phrases.reflow(width);
    assert(phrases.pageCount() == 2 && phrases.visibleCount() == 5);
    phrases.turnPage(1); assert(phrases.pageStart() == 5 && phrases.visibleCount() == 5);
    checkGeometry(phrases);

    ime::State mixed;
    mixed.candidates = {"苹", "苹果", "中华人民共和国", "平", "你好", "词典"};
    mixed.reflow(width);
    assert(mixed.visibleCount() == 4 && mixed.pageCount() == 2);
    checkGeometry(mixed);
    mixed.reflow([](const std::string& text) { return text == "1" ? 4 : width(text); });
    checkGeometry(mixed);

    ime::State longWord;
    longWord.candidates = {"苹", ""};
    for (int i = 0; i < 30; ++i) longWord.candidates[1] += "汉";
    longWord.reflow(width);
    assert(longWord.pageCount() == 2);
    assert(longWord.placements[1].x == 0 && longWord.placements[1].wordWidth == 223);
    checkGeometry(longWord);

    ime::State numbered;
    numbered.candidates.assign(20, "a");
    numbered.reflow(width);
    assert(numbered.visibleCount() == 9 && numbered.pageCount() == 3);
    checkGeometry(numbered);
    numbered.turnPage(2);
    numbered.candidates = {"苹果"};
    numbered.reflow(width);
    assert(numbered.page == 0 && numbered.visibleCount() == 1);
    numbered.candidates.clear(); numbered.reflow(width);
    std::string query;
    assert(numbered.pageCount() == 1 && !numbered.select(0, query));
    std::cout << "Single-line candidate layout passed: dense packing, variable page sizes, numbering, UTF-8 and long-word selection.\n";
}
