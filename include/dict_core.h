#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace dict {

constexpr size_t kMaxWordBytes = 255;
constexpr size_t kMaxDefinitionBytes = 12288;
constexpr uint32_t kStride = 64;

inline uint8_t fold(uint8_t c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
inline int compare(const std::string& a, const std::string& b) {
    for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
        const int d = int(fold(uint8_t(a[i]))) - int(fold(uint8_t(b[i])));
        if (d) return d;
    }
    return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
}
inline int indexCompare(const std::string& a, const std::string& b) {
    const int d = compare(a, b);
    return d ? d : a.compare(b);
}
inline bool startsWith(const std::string& word, const std::string& prefix) {
    return word.size() >= prefix.size() && compare(word.substr(0, prefix.size()), prefix) == 0;
}
inline size_t utf8Length(uint8_t c) {
    return c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
}
inline void popUtf8(std::string& value) {
    if (value.empty()) return;
    size_t i = value.size() - 1;
    while (i && (uint8_t(value[i]) & 0xc0) == 0x80) --i;
    value.resize(i);
}
inline std::string utf8At(const std::string& text, size_t index) {
    size_t pos = 0;
    while (pos < text.size() && index) { pos += utf8Length(uint8_t(text[pos])); --index; }
    return pos < text.size() ? text.substr(pos, utf8Length(uint8_t(text[pos]))) : "";
}
inline size_t utf8Count(const std::string& text) {
    size_t count = 0;
    for (size_t i = 0; i < text.size(); i += utf8Length(uint8_t(text[i]))) ++count;
    return count;
}

struct Metadata {
    uint32_t count = 0;
    uint32_t indexBytes = 0;
    std::string name;
};
inline bool decimal(const std::string& text, uint32_t& result) {
    if (text.empty()) return false;
    uint64_t n = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + uint32_t(c - '0');
        if (n > UINT32_MAX) return false;
    }
    result = uint32_t(n);
    return true;
}
inline bool parseMetadata(const std::string& source, Metadata& meta, std::string& error) {
    meta = {};
    if (source.rfind("StarDict's dict ifo file", 0) != 0) { error = "Invalid StarDict .ifo header"; return false; }
    std::string version, types, offsetBits = "32";
    size_t pos = 0;
    while (pos < source.size()) {
        const auto end = source.find('\n', pos);
        auto line = source.substr(pos, end == std::string::npos ? end : end - pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const auto eq = line.find('=');
        if (eq != std::string::npos) {
            const auto key = line.substr(0, eq), value = line.substr(eq + 1);
            if (key == "version") version = value;
            else if (key == "bookname") meta.name = value;
            else if (key == "sametypesequence") types = value;
            else if (key == "idxoffsetbits") offsetBits = value;
            else if (key == "wordcount" && !decimal(value, meta.count)) { error = "Invalid wordcount"; return false; }
            else if (key == "idxfilesize" && !decimal(value, meta.indexBytes)) { error = "Invalid idxfilesize"; return false; }
        }
        if (end == std::string::npos) break;
        pos = end + 1;
    }
    if (version != "2.4.2" && version != "3.0.0") { error = "Unsupported StarDict version"; return false; }
    if (types != "m" || offsetBits != "32") { error = "Use plain text (m), 32-bit offsets"; return false; }
    if (!meta.count || meta.count > 1000000 || !meta.indexBytes) { error = "Wordcount must be 1..1000000"; return false; }
    return true;
}

class Reader {
public:
    virtual ~Reader() = default;
    virtual uint32_t size() const = 0;
    virtual size_t readAt(uint32_t offset, uint8_t* buffer, size_t length) = 0;
};

class BufferedReader {
public:
    void reset(Reader* reader) { reader_ = reader; length_ = reader ? reader->size() : 0; cached_ = 0; start_ = 0; }
    bool byte(uint32_t offset, uint8_t& value) {
        if (!reader_ || offset >= length_) return false;
        if (!cached_ || offset < start_ || offset - start_ >= cached_) {
            start_ = offset;
            cached_ = reader_->readAt(offset, buffer_, sizeof(buffer_));
            if (!cached_) return false;
        }
        value = buffer_[offset - start_];
        return true;
    }
private:
    Reader* reader_ = nullptr;
    uint8_t buffer_[1024] = {};
    uint32_t start_ = 0;
    uint32_t length_ = 0;
    size_t cached_ = 0;
};

struct Entry {
    std::string word;
    uint32_t offset = 0;
    uint32_t length = 0;
    uint32_t next = 0;
};

inline uint32_t crc32(const uint8_t* bytes, size_t length, uint32_t state = 0xffffffffu) {
    for (size_t i = 0; i < length; ++i) {
        state ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) state = (state >> 1) ^ (0xedb88320u & (0u - (state & 1u)));
    }
    return state;
}

// File timestamps plus bounded content fingerprints keep cache checks small.
// A full reindex is still available for externally edited files with preserved
// timestamps and modifications that fall outside the sampled regions.
struct CacheIdentity {
    uint32_t ifoCrc = 0;
    uint32_t ifoModified = 0;
    uint32_t indexModified = 0;
    uint32_t bodyModified = 0;
    uint32_t indexSampleCrc = 0;
    uint32_t bodySampleCrc = 0;
};

inline bool sampleCrc(Reader& source, uint32_t& result) {
    uint32_t state = 0xffffffffu;
    uint8_t bytes[512];
    const uint32_t size = source.size();
    const size_t length = std::min(size, uint32_t(sizeof(bytes)));
    for (uint32_t i = 0; i < (size > sizeof(bytes) ? 8u : 1u); ++i) {
        const uint32_t pos = size > sizeof(bytes) ? uint64_t(size - length) * i / 7 : 0;
        if (length && source.readAt(pos, bytes, length) != length) return false;
        state = crc32(bytes, length, state);
    }
    result = ~state;
    return true;
}

constexpr size_t kCacheHeaderBytes = 64;
inline uint32_t little32(const uint8_t* bytes) {
    return uint32_t(bytes[0]) | (uint32_t(bytes[1]) << 8) | (uint32_t(bytes[2]) << 16) | (uint32_t(bytes[3]) << 24);
}
inline void putLittle32(uint8_t* bytes, uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[i] = uint8_t(value >> (i * 8));
}

class StarDict {
public:
    bool open(Reader& index, Reader& body, const Metadata& meta,
              std::string& error, std::function<void(uint32_t, uint32_t)> progress = {}) {
        ready_ = false;
        anchors_.clear();
        index_ = &index; body_ = &body; meta_ = meta; cached_.reset(index_);
        if (meta.count == 0 || meta.count > 1000000 || index.size() != meta.indexBytes) {
            error = "Index size / wordcount mismatch"; return false;
        }
        anchors_.reserve((meta.count + kStride - 1) / kStride);
        uint32_t pos = 0;
        std::string previous;
        for (uint32_t i = 0; i < meta.count; ++i) {
            Entry entry;
            if (!record(pos, entry)) { error = "Invalid index or definition range"; return false; }
            if (i && indexCompare(previous, entry.word) > 0) { error = "Index is not in StarDict order"; return false; }
            if (i % kStride == 0) anchors_.push_back(pos);
            previous = entry.word;
            pos = entry.next;
            if (progress && i % 1024 == 0) progress(i, meta.count);
        }
        if (pos != index.size()) { error = "Extra index bytes / wrong wordcount"; return false; }
        ready_ = true;
        if (progress) progress(meta.count, meta.count);
        return true;
    }

    std::vector<uint8_t> serializeIndex(const CacheIdentity& identity) const {
        if (!ready_) return {};
        std::vector<uint8_t> data(kCacheHeaderBytes + anchors_.size() * 4, 0);
        memcpy(data.data(), "CPDIDX01", 8);
        const uint32_t fields[] = {1, kStride, meta_.count, index_->size(), body_->size(), uint32_t(anchors_.size()),
            identity.ifoCrc, identity.ifoModified, identity.indexModified, identity.bodyModified, identity.indexSampleCrc, identity.bodySampleCrc};
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) putLittle32(data.data() + 8 + i * 4, fields[i]);
        for (size_t i = 0; i < anchors_.size(); ++i) putLittle32(data.data() + kCacheHeaderBytes + i * 4, anchors_[i]);
        putLittle32(data.data() + 56, ~crc32(data.data() + kCacheHeaderBytes, data.size() - kCacheHeaderBytes));
        putLittle32(data.data() + 60, ~crc32(data.data(), 60));
        return data;
    }

    bool restoreIndex(Reader& index, Reader& body, const Metadata& meta, const CacheIdentity& identity,
                      const std::vector<uint8_t>& data, std::string& error) {
        ready_ = false; anchors_.clear();
        index_ = &index; body_ = &body; meta_ = meta; cached_.reset(index_);
        if (!meta.count || meta.count > 1000000 || index.size() != meta.indexBytes || data.size() < kCacheHeaderBytes) {
            error = "Invalid cached index size / metadata"; return false;
        }
        const uint32_t count = (meta.count + kStride - 1) / kStride;
        if (data.size() != kCacheHeaderBytes + size_t(count) * 4 || memcmp(data.data(), "CPDIDX01", 8) != 0) {
            error = "Invalid cached index format / length"; return false;
        }
        if (little32(data.data() + 60) != ~crc32(data.data(), 60) ||
            little32(data.data() + 56) != ~crc32(data.data() + kCacheHeaderBytes, data.size() - kCacheHeaderBytes)) {
            error = "Cached index checksum mismatch"; return false;
        }
        const uint32_t expected[] = {1, kStride, meta.count, index.size(), body.size(), count,
            identity.ifoCrc, identity.ifoModified, identity.indexModified, identity.bodyModified, identity.indexSampleCrc, identity.bodySampleCrc};
        for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
            if (little32(data.data() + 8 + i * 4) != expected[i]) {
                error = "Cached index belongs to a different dictionary"; return false;
            }
        }
        uint32_t previous = 0;
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t position = little32(data.data() + kCacheHeaderBytes + i * 4);
            if ((i == 0 && position != 0) || (i && position <= previous) || position >= index.size()) {
                error = "Invalid cached index anchor"; return false;
            }
            previous = position;
        }
        // Spot-check a bounded number of anchors against the source index.
        std::string previousWord;
        uint32_t previousAnchor = UINT32_MAX;
        for (uint32_t i = 0; i < std::min(count, 8u); ++i) {
            const uint32_t anchor = count <= 8 ? i : uint64_t(count - 1) * i / 7;
            const uint32_t position = little32(data.data() + kCacheHeaderBytes + anchor * 4);
            uint8_t preceding = 0;
            Entry entry;
            if ((position && index.readAt(position - 9, &preceding, 1) != 1) ||
                (position && preceding != 0) || !record(position, entry) ||
                (previousAnchor != UINT32_MAX && indexCompare(previousWord, entry.word) > 0)) {
                error = "Cached anchor does not match source index"; return false;
            }
            previousWord = entry.word; previousAnchor = anchor;
        }
        anchors_.reserve(count);
        for (uint32_t i = 0; i < count; ++i) anchors_.push_back(little32(data.data() + kCacheHeaderBytes + i * 4));
        ready_ = true;
        return true;
    }

    bool ready() const { return ready_; }
    uint32_t count() const { return meta_.count; }
    size_t indexRamBytes() const { return anchors_.size() * sizeof(uint32_t); }

    std::vector<Entry> suggest(const std::string& query, size_t limit = 5) {
        std::vector<Entry> result;
        if (!ready_ || !limit) return result;
        // Start at the anchor immediately BEFORE the first >= query, preserving
        // case-insensitive duplicates that can straddle an anchor boundary.
        size_t lo = 0, hi = anchors_.size();
        Entry item;
        while (lo < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (!record(anchors_[mid], item)) return {};
            if (compare(item.word, query) < 0) lo = mid + 1; else hi = mid;
        }
        uint32_t pos = anchors_[lo ? lo - 1 : 0];
        while (pos < index_->size()) {
            if (!record(pos, item)) break;
            pos = item.next;
            if (compare(item.word, query) < 0) continue;
            result.push_back(item);
            if (result.size() == limit) break;
        }
        return result;
    }

    std::string definition(const Entry& entry, bool& truncated, bool& ok) {
        truncated = entry.length > kMaxDefinitionBytes;
        ok = false;
        if (!body_ || entry.offset > body_->size() || entry.length > body_->size() - entry.offset) return "";
        const size_t n = std::min(size_t(entry.length), kMaxDefinitionBytes);
        std::string text(n, '\0');
        if (n && body_->readAt(entry.offset, reinterpret_cast<uint8_t*>(&text[0]), n) != n) return "";
        if (truncated) {
            // Drop a partial final UTF-8 character without dropping a complete one.
            size_t tail = text.size() - 1;
            while (tail && (uint8_t(text[tail]) & 0xc0) == 0x80) --tail;
            if (tail + utf8Length(uint8_t(text[tail])) > text.size()) text.resize(tail);
        }
        ok = true;
        return text;
    }

private:
    bool record(uint32_t pos, Entry& entry) {
        entry = {};
        uint8_t c = 0;
        bool terminated = false;
        for (size_t i = 0; i <= kMaxWordBytes; ++i) {
            if (!cached_.byte(pos++, c)) return false;
            if (!c) { terminated = true; break; }
            entry.word.push_back(char(c));
        }
        if (!terminated || entry.word.empty()) return false;
        uint32_t values[2] = {};
        for (auto& value : values) for (int i = 0; i < 4; ++i) {
            if (!cached_.byte(pos++, c)) return false;
            value = (value << 8) | c;
        }
        entry.offset = values[0]; entry.length = values[1]; entry.next = pos;
        return body_ && entry.offset <= body_->size() && entry.length <= body_->size() - entry.offset;
    }
    Reader* index_ = nullptr;
    Reader* body_ = nullptr;
    BufferedReader cached_;
    Metadata meta_;
    std::vector<uint32_t> anchors_;
    bool ready_ = false;
};

} // namespace dict
