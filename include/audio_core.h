#pragma once
#include "dict_core.h"

namespace pronunciation {
constexpr uint32_t kMaxAudioBytes = 512 * 1024;
constexpr uint32_t kMaxDurationSeconds = 30;

inline bool normalize(const std::string& source, std::string& word) {
    word.clear();
    bool space = false;
    for (const auto value : source) {
        const uint8_t c = dict::fold(uint8_t(value));
        if (c == ' ' || c == '\t') { if (!word.empty()) space = true; continue; }
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '\'' || c == '-' || c == '.')) return false;
        if (space) { word += ' '; space = false; }
        word += char(c);
        if (word.size() > 64) return false;
    }
    return !word.empty() && std::any_of(word.begin(), word.end(), [](char c) { return c >= 'a' && c <= 'z'; });
}

inline std::string encode(const std::string& word) {
    constexpr char hex[] = "0123456789abcdef";
    std::string result;
    for (const uint8_t c : word) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') result += char(c);
        else { result += '%'; result += hex[c >> 4]; result += hex[c & 15]; }
    }
    return result;
}

inline uint16_t le16(const uint8_t* data) { return uint16_t(data[0]) | (uint16_t(data[1]) << 8); }

struct Wave {
    uint32_t offset = 0, bytes = 0, rate = 0;
    uint16_t channels = 0, bits = 0;
};

inline bool parseWave(dict::Reader& source, Wave& wave) {
    wave = {};
    uint8_t header[40] = {};
    if (source.size() < 44 || source.size() > kMaxAudioBytes || source.readAt(0, header, 12) != 12 ||
        memcmp(header, "RIFF", 4) || memcmp(header + 8, "WAVE", 4)) return false;
    const uint64_t end = uint64_t(dict::little32(header + 4)) + 8;
    if (end > source.size() || end < 44) return false;
    bool haveFormat = false, haveData = false;
    for (uint64_t pos = 12; pos + 8 <= end;) {
        if (source.readAt(uint32_t(pos), header, 8) != 8) return false;
        const uint32_t length = dict::little32(header + 4);
        const uint64_t next = pos + 8 + length + (length & 1);
        if (next > end) return false;
        if (memcmp(header, "fmt ", 4) == 0) {
            if (haveFormat || length < 16 || source.readAt(uint32_t(pos + 8), header, 16) != 16 || le16(header) != 1) return false;
            wave.channels = le16(header + 2); wave.rate = dict::little32(header + 4); wave.bits = le16(header + 14);
            if ((wave.channels != 1 && wave.channels != 2) || (wave.bits != 8 && wave.bits != 16) ||
                wave.rate < 8000 || wave.rate > 48000 || le16(header + 12) != wave.channels * wave.bits / 8 ||
                dict::little32(header + 8) != wave.rate * wave.channels * wave.bits / 8) return false;
            haveFormat = true;
        } else if (memcmp(header, "data", 4) == 0) {
            if (haveData) return false;
            wave.offset = uint32_t(pos + 8); wave.bytes = length; haveData = true;
        }
        pos = next;
    }
    if (!haveFormat || !haveData || !wave.bytes || wave.bytes % (wave.channels * wave.bits / 8) != 0) return false;
    return wave.bytes <= wave.rate * wave.channels * (wave.bits / 8) * kMaxDurationSeconds;
}
} // namespace pronunciation
