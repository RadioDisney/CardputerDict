#include "dict_core.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>

class MemoryReader : public dict::Reader {
public:
    std::vector<uint8_t> bytes;
    size_t bytesRead = 0;
    uint32_t size() const override { return bytes.size(); }
    size_t readAt(uint32_t pos, uint8_t* destination, size_t length) override {
        if (pos > bytes.size()) return 0;
        length = std::min(length, bytes.size() - pos);
        memcpy(destination, bytes.data() + pos, length); bytesRead += length; return length;
    }
};

class FileReader : public dict::Reader {
public:
    explicit FileReader(const std::string& name) : file(name, std::ios::binary) {
        assert(file); file.seekg(0, std::ios::end); length = file.tellg();
    }
    uint32_t size() const override { return length; }
    size_t readAt(uint32_t pos, uint8_t* destination, size_t count) override {
        file.clear(); file.seekg(pos); file.read(reinterpret_cast<char*>(destination), count);
        return size_t(file.gcount());
    }
private:
    std::ifstream file;
    uint32_t length;
};

void big32(std::vector<uint8_t>& data, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) data.push_back(uint8_t(value >> shift));
}

dict::CacheIdentity identity(dict::Reader& index, dict::Reader& body) {
    dict::CacheIdentity result{123, 456, 789, 1011, 0, 0};
    assert(dict::sampleCrc(index, result.indexSampleCrc));
    assert(dict::sampleCrc(body, result.bodySampleCrc));
    return result;
}

void checksum(std::vector<uint8_t>& data) {
    dict::putLittle32(data.data() + 56, ~dict::crc32(data.data() + dict::kCacheHeaderBytes, data.size() - dict::kCacheHeaderBytes));
    dict::putLittle32(data.data() + 60, ~dict::crc32(data.data(), 60));
}

void compareQueries(dict::StarDict& full, dict::StarDict& cached, const std::string& query) {
    auto a = full.suggest(query), b = cached.suggest(query);
    assert(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        assert(a[i].word == b[i].word && a[i].offset == b[i].offset && a[i].length == b[i].length && a[i].next == b[i].next);
        bool ta, tb, oa, ob;
        assert(full.definition(a[i], ta, oa) == cached.definition(b[i], tb, ob));
        assert(oa && ob && ta == tb);
    }
}

int main(int argc, char** argv) {
    const std::string standard = "123456789";
    assert(~dict::crc32(reinterpret_cast<const uint8_t*>(standard.data()), standard.size()) == 0xcbf43926u);
    std::vector<std::string> words{"Apple", "apple", "苹果", "你好"};
    for (int i = 0; i < 6000; ++i) {
        char word[32]; snprintf(word, sizeof(word), "word%05d", i); words.emplace_back(word);
    }
    for (int i = 0; i < 140; ++i) words.emplace_back("word02000");
    std::sort(words.begin(), words.end(), [](const auto& a, const auto& b) { return dict::indexCompare(a, b) < 0; });
    MemoryReader index, body;
    for (const auto& word : words) {
        const std::string text = "meaning: " + word;
        index.bytes.insert(index.bytes.end(), word.begin(), word.end()); index.bytes.push_back(0);
        big32(index.bytes, body.size()); big32(index.bytes, text.size());
        body.bytes.insert(body.bytes.end(), text.begin(), text.end());
    }
    dict::Metadata meta{uint32_t(words.size()), index.size(), "test"};
    std::string error;
    dict::StarDict full, restored;
    assert(full.serializeIndex({}).empty());
    assert(full.open(index, body, meta, error));
    const auto id = identity(index, body);
    const auto saved = full.serializeIndex(id);
    assert(saved.size() == 64 + ((words.size() + 63) / 64) * 4);
    index.bytesRead = body.bytesRead = 0;
    assert(restored.restoreIndex(index, body, meta, id, saved, error));
    assert(index.bytesRead <= 8 * 1025 && body.bytesRead == 0);
    assert(restored.serializeIndex(id) == saved);
    std::mt19937 random(99);
    for (int i = 0; i < 10000; ++i) compareQueries(full, restored, words[random() % words.size()]);
    for (const auto& query : {"", "app", "WORD02000", "word99999", "苹果", "你好"}) compareQueries(full, restored, query);

    auto rejected = [&](const std::vector<uint8_t>& data, const dict::CacheIdentity& sourceId) {
        assert(!restored.restoreIndex(index, body, meta, sourceId, data, error));
        assert(!restored.ready() && restored.suggest("apple").empty());
    };
    for (size_t i = 0; i < saved.size(); ++i) {
        auto corrupt = saved; corrupt[i] ^= 0x40; rejected(corrupt, id);
    }
    for (size_t size : {size_t(0), size_t(4), size_t(63), size_t(64), saved.size() - 1}) {
        rejected(std::vector<uint8_t>(saved.begin(), saved.begin() + size), id);
    }
    auto extra = saved; extra.push_back(0); rejected(extra, id);
    for (int field = 0; field < 6; ++field) {
        auto modified = id;
        if (field == 0) ++modified.ifoCrc;
        if (field == 1) ++modified.ifoModified;
        if (field == 2) ++modified.indexModified;
        if (field == 3) ++modified.bodyModified;
        if (field == 4) ++modified.indexSampleCrc;
        if (field == 5) ++modified.bodySampleCrc;
        rejected(saved, modified);
    }
    for (size_t offset : {size_t(8), size_t(12), size_t(16), size_t(20), size_t(24), size_t(28)}) {
        auto invalid = saved; dict::putLittle32(invalid.data() + offset, 0xffffffffu); checksum(invalid); rejected(invalid, id);
    }
    for (auto value : {1u, 0xffffffffu}) {
        auto invalid = saved; dict::putLittle32(invalid.data() + 64, value); checksum(invalid); rejected(invalid, id);
    }
    auto descending = saved; dict::putLittle32(descending.data() + 72, dict::little32(descending.data() + 68)); checksum(descending); rejected(descending, id);
    auto wrongBoundary = saved;
    const size_t finalAnchor = wrongBoundary.size() - 4;
    dict::putLittle32(wrongBoundary.data() + finalAnchor, dict::little32(wrongBoundary.data() + finalAnchor) + 1);
    checksum(wrongBoundary); rejected(wrongBoundary, id);
    const auto original = index.bytes[0]; index.bytes[0] ^= 1;
    rejected(saved, identity(index, body)); index.bytes[0] = original;
    body.bytes.back() ^= 1; rejected(saved, identity(index, body)); body.bytes.back() ^= 1;
    body.bytes.push_back(0); rejected(saved, id); body.bytes.pop_back();
    // A cache failure must permit rebuilding from the unchanged source.
    assert(restored.open(index, body, meta, error));
    assert(restored.serializeIndex(id) == saved);
    MemoryReader small; small.bytes = {1, 2, 3}; uint32_t sample = 0;
    assert(dict::sampleCrc(small, sample) && sample == ~dict::crc32(small.bytes.data(), 3));

    if (argc == 2) {
        const std::string prefix = argv[1];
        std::ifstream ifo(prefix + ".ifo");
        const std::string source((std::istreambuf_iterator<char>(ifo)), {});
        assert(dict::parseMetadata(source, meta, error));
        FileReader actualIndex(prefix + ".idx"), actualBody(prefix + ".dict");
        const auto actualId = identity(actualIndex, actualBody);
        assert(full.open(actualIndex, actualBody, meta, error));
        const auto data = full.serializeIndex(actualId);
        assert(restored.restoreIndex(actualIndex, actualBody, meta, actualId, data, error));
        for (const auto& query : {"", "apple", "TESLA", "run", "苹果", "nihao", "pingguo", "zzzzzz"}) compareQueries(full, restored, query);
        std::cout << "Real serialized dictionary validated: " << meta.count << " headwords, " << data.size() << " cache bytes\n";
    }
    std::cout << "Index cache tests passed: 10000 equivalent lookups, every-byte corruption, truncation, metadata changes, source changes and rebuild.\n";
}
