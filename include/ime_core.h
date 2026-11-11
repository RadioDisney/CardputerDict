#pragma once
#include "dict_core.h"

namespace ime {
constexpr int kMaxPageCandidates = 9;
constexpr size_t kMaxComposition = 32;
constexpr size_t kMaxCandidates = 96;

inline std::string normalize(const std::string& input) {
    std::string result;
    for (const auto c : input) {
        if (c == '\'' || c == ' ') continue;
        if (c == ':' && !result.empty() && result.back() == 'u') { result.back() = 'v'; continue; }
        const auto lower = dict::fold(uint8_t(c));
        if (lower < 'a' || lower > 'z') return "";
        result += char(lower);
    }
    return result;
}

inline bool hasHan(const std::string& text) {
    for (size_t i = 0; i + 2 < text.size(); ++i) {
        const auto a = uint8_t(text[i]), b = uint8_t(text[i + 1]), c = uint8_t(text[i + 2]);
        if ((a & 0xf0) == 0xe0 && (b & 0xc0) == 0x80 && (c & 0xc0) == 0x80) {
            const uint32_t code = ((a & 15) << 12) | ((b & 63) << 6) | (c & 63);
            if (code >= 0x3400 && code <= 0x9fff) return true;
        }
    }
    return false;
}

inline void addUnique(std::vector<std::string>& words, const std::string& word) {
    if (!word.empty() && words.size() < kMaxCandidates &&
        std::find(words.begin(), words.end(), word) == words.end()) words.push_back(word);
}

inline void addLines(std::vector<std::string>& words, const std::string& text, size_t limit = kMaxCandidates) {
    size_t start = 0, count = 0;
    while (start < text.size() && count++ < limit) {
        const auto end = text.find('\n', start);
        addUnique(words, text.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) break;
        start = end + 1;
    }
}

struct State {
    struct Placement {
        int x;
        int labelWidth;
        int wordWidth;
    };
    std::string input;
    std::vector<std::string> candidates;
    std::vector<Placement> placements;
    std::vector<size_t> pageStarts;
    int page = 0;
    void resetLayout() { placements.clear(); pageStarts.clear(); page = 0; }
    template <typename Measure>
    void reflow(Measure measure, int lineWidth = 232) {
        resetLayout();
        lineWidth = std::max(1, lineWidth);
        pageStarts.push_back(0);
        int x = 0, count = 0;
        for (size_t i = 0; i < candidates.size(); ++i) {
            const std::string label = std::to_string(count + 1);
            int labelWidth = std::max(1, measure(label));
            const int naturalWidth = std::max(1, measure(candidates[i]));
            int wordWidth = std::min(naturalWidth, std::max(1, lineWidth - labelWidth - 1));
            int itemWidth = labelWidth + 1 + wordWidth;
            if (count == kMaxPageCandidates || (x && x + itemWidth > lineWidth)) {
                pageStarts.push_back(i); x = count = 0;
                labelWidth = std::max(1, measure("1"));
                wordWidth = std::min(naturalWidth, std::max(1, lineWidth - labelWidth - 1));
                itemWidth = labelWidth + 1 + wordWidth;
            }
            placements.push_back({x, labelWidth, wordWidth});
            x += itemWidth + 4;
            ++count;
        }
        pageStarts.push_back(candidates.size());
    }
    size_t pageCount() const { return pageStarts.size() > 1 ? pageStarts.size() - 1 : 1; }
    size_t pageStart() const {
        return page >= 0 && size_t(page) + 1 < pageStarts.size() ? pageStarts[page] : 0;
    }
    size_t visibleCount() const {
        if (page >= 0 && size_t(page) + 1 < pageStarts.size())
            return pageStarts[page + 1] - pageStarts[page];
        return std::min(candidates.size(), size_t(kMaxPageCandidates));
    }
    bool append(char c) {
        if (input.size() >= kMaxComposition) return false;
        c = char(dict::fold(uint8_t(c)));
        if ((c < 'a' || c > 'z') && c != '\'') return false;
        input += c; page = 0; return true;
    }
    void turnPage(int delta) {
        const int last = int(pageCount() - 1);
        page = std::max(0, std::min(last, page + delta));
    }
    bool select(int number, std::string& query, size_t maxBytes = 120) {
        if (number < 0 || number >= kMaxPageCandidates || size_t(number) >= visibleCount()) return false;
        const size_t i = pageStart() + number;
        if (i >= candidates.size() || query.size() + candidates[i].size() > maxBytes) return false;
        query += candidates[i];
        input.clear(); candidates.clear(); resetLayout();
        return true;
    }
};
} // namespace ime
