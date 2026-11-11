#include "dict_core.h"
#include "ime_core.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>

class MemoryReader : public dict::Reader {
public:
    std::vector<uint8_t> bytes;
    uint32_t size() const override { return bytes.size(); }
    size_t readAt(uint32_t offset, uint8_t* target, size_t length) override {
        if (offset >= bytes.size()) return 0;
        length = std::min(length, bytes.size() - offset);
        memcpy(target, bytes.data() + offset, length);
        return length;
    }
};

class FileReader : public dict::Reader {
public:
    explicit FileReader(const std::string& path) : file(path, std::ios::binary) {
        assert(file.good()); file.seekg(0, std::ios::end); length = file.tellg();
    }
    uint32_t size() const override { return length; }
    size_t readAt(uint32_t offset, uint8_t* target, size_t count) override {
        file.clear(); file.seekg(offset); file.read(reinterpret_cast<char*>(target), count); return file.gcount();
    }
private:
    std::ifstream file;
    uint32_t length;
};

void be32(std::vector<uint8_t>& bytes, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) bytes.push_back((value >> shift) & 255);
}

int main(int argc, char** argv) {
    assert(ime::normalize("Xi'An") == "xian");
    assert(ime::normalize("LU:E") == "lve");
    assert(ime::normalize("ni3hao") == "");
    assert(ime::hasHan("苹果") && ime::hasHan("蘋果") && !ime::hasHan("apple"));
    ime::State composer;
    for (char c : std::string("pingguo")) assert(composer.append(c));
    ime::addLines(composer.candidates, "苹果\n平果\n苹果\n评估\n凭据");
    assert(composer.candidates.size() == 4);
    std::string committed;
    assert(composer.select(0, committed));
    assert(committed == "苹果" && composer.input.empty() && composer.candidates.empty());
    dict::popUtf8(committed); assert(committed == "苹");
    composer.input = "guo"; composer.candidates = {"国", "过", "果", "锅"};
    composer.turnPage(1); assert(composer.page == 1);
    assert(!composer.select(1, committed));
    assert(composer.select(0, committed) && committed == "苹锅");
    composer.input = "guo"; composer.candidates = {"果"};
    assert(!composer.select(0, committed, committed.size()));
    assert(composer.input == "guo" && composer.candidates[0] == "果");
    composer.input.assign(ime::kMaxComposition, 'a'); assert(!composer.append('b'));
    assert(dict::compare("Apple", "apple") == 0);
    assert(dict::compare("a", "ab") < 0);
    assert(dict::indexCompare("APPLE", "apple") < 0);
    std::string unicode = "a苹果";
    dict::popUtf8(unicode); assert(unicode == "a苹");
    dict::popUtf8(unicode); assert(unicode == "a");
    assert(dict::utf8At("你好世界", 2) == "世");
    assert(dict::utf8Count("a你好") == 3);
    dict::Metadata meta;
    std::string error;
    const std::string header = "StarDict's dict ifo file\r\nversion=2.4.2\r\nwordcount=1\r\nidxfilesize=14\r\nbookname=Test\r\nsametypesequence=m\r\n";
    assert(dict::parseMetadata(header, meta, error));
    assert(meta.count == 1 && meta.indexBytes == 14);
    assert(!dict::parseMetadata(header + "idxoffsetbits=64\n", meta, error));
    assert(!dict::parseMetadata(header + "sametypesequence=h\n", meta, error));
    assert(!dict::parseMetadata(header + "wordcount=-1\n", meta, error));
    assert(!dict::parseMetadata(header + "wordcount=4294967296\n", meta, error));

    std::vector<std::string> words;
    for (int i = 0; i < 4000; ++i) { char key[32]; snprintf(key, sizeof(key), "word%05d", i); words.emplace_back(key); }
    // A duplicate group spanning multiple sparse anchors must return its FIRST item.
    for (int i = 0; i < 140; ++i) words.emplace_back("word02000");
    words.emplace_back("Apple"); words.emplace_back("apple"); words.emplace_back("苹果");
    std::sort(words.begin(), words.end(), [](const auto& a, const auto& b) { return dict::indexCompare(a, b) < 0; });
    MemoryReader index, body;
    std::vector<uint32_t> offsets;
    for (const auto& word : words) {
        const std::string text = "meaning: " + word;
        offsets.push_back(body.bytes.size());
        index.bytes.insert(index.bytes.end(), word.begin(), word.end()); index.bytes.push_back(0);
        be32(index.bytes, body.bytes.size()); be32(index.bytes, text.size());
        body.bytes.insert(body.bytes.end(), text.begin(), text.end());
    }
    meta.count = words.size(); meta.indexBytes = index.size();
    dict::StarDict dictionary;
    assert(dictionary.open(index, body, meta, error));
    assert(dictionary.indexRamBytes() == ((words.size() + 63) / 64) * 4);
    std::mt19937 random(42);
    for (int i = 0; i < 10000; ++i) {
        const auto query = i % 7 == 0 ? std::string("word02000") : words[random() % words.size()];
        const auto found = dictionary.suggest(query);
        const auto expected = std::lower_bound(words.begin(), words.end(), query, [](const auto& a, const auto& b) { return dict::compare(a, b) < 0; });
        assert(!found.empty());
        const auto position = std::distance(words.begin(), expected);
        assert(found[0].word == *expected && found[0].offset == offsets[position]);
        bool truncated = false, ok = false;
        assert(dictionary.definition(found[0], truncated, ok) == "meaning: " + found[0].word);
        assert(ok && !truncated);
    }
    assert(dictionary.suggest("a")[0].word == "Apple");
    assert(dictionary.suggest("龘").empty());
    assert(dictionary.suggest("", 0).empty());
    auto saved = index.bytes;
    index.bytes.pop_back(); assert(!dictionary.open(index, body, meta, error));
    index.bytes = saved;
    body.bytes.clear(); assert(!dictionary.open(index, body, meta, error));
    // UTF-8 truncation must preserve complete codepoints.
    MemoryReader longIndex, longBody;
    std::string longText(dict::kMaxDefinitionBytes - 1, 'a'); longText += "苹果";
    longBody.bytes.assign(longText.begin(), longText.end());
    longIndex.bytes = {'x', 0}; be32(longIndex.bytes, 0); be32(longIndex.bytes, longBody.size());
    meta.count = 1; meta.indexBytes = longIndex.size();
    assert(dictionary.open(longIndex, longBody, meta, error));
    bool truncated, ok;
    auto text = dictionary.definition(dictionary.suggest("x")[0], truncated, ok);
    assert(ok && truncated && text.size() == dict::kMaxDefinitionBytes - 1);

    if (argc >= 2) {
        const std::string prefix = argv[1];
        std::ifstream info(prefix + ".ifo");
        std::string source((std::istreambuf_iterator<char>(info)), {});
        assert(dict::parseMetadata(source, meta, error));
        FileReader actualIndex(prefix + ".idx"), actualBody(prefix + ".dict");
        if (!dictionary.open(actualIndex, actualBody, meta, error)) { std::cerr << error << "\n"; return 1; }
        const std::string lookup = argc > 2 ? argv[2] : "apple";
        const std::string expected = argc > 3 ? argv[3] : "苹果";
        auto result = dictionary.suggest(lookup);
        assert(!result.empty() && dict::compare(result[0].word, lookup) == 0);
        const auto text = dictionary.definition(result[0], truncated, ok);
        assert(ok && text.find(expected) != std::string::npos);
        std::cout << "Validated real dictionary: " << meta.count << " headwords, " << dictionary.indexRamBytes() << " sparse index bytes\n";
    }
    std::cout << "Core tests passed: 10000 reference lookups, duplicate boundaries, malformed data, Chinese UTF-8.\n";
}
