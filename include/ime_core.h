#pragma once
#include "dict_core.h"

namespace ime {
constexpr int kPageSize = 3;
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
    std::string input;
    std::vector<std::string> candidates;
    int page = 0;
    bool append(char c) {
        if (input.size() >= kMaxComposition) return false;
        c = char(dict::fold(uint8_t(c)));
        if ((c < 'a' || c > 'z') && c != '\'') return false;
        input += c; page = 0; return true;
    }
    void turnPage(int delta) {
        const int last = candidates.empty() ? 0 : int((candidates.size() - 1) / kPageSize);
        page = std::max(0, std::min(last, page + delta));
    }
    bool select(int number, std::string& query, size_t maxBytes = 120) {
        if (number < 0 || number >= kPageSize) return false;
        const size_t i = size_t(page * kPageSize + number);
        if (i >= candidates.size() || query.size() + candidates[i].size() > maxBytes) return false;
        query += candidates[i];
        input.clear(); candidates.clear(); page = 0;
        return true;
    }
};
} // namespace ime
