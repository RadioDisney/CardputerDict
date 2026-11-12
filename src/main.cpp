#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "dict_core.h"
#include "demo_data.h"
#include "pinyin_data.h"
#include "body_font.h"
#include "ime_core.h"
#include "ui_text.h"
#include "pronunciation.h"
#include "key_repeat.h"
#include "keyboard_input.h"
#include <esp_heap_caps.h>

namespace {
constexpr uint16_t BG = 0x0863, FG = 0xef7d, MUTED = 0x8c92;
constexpr uint16_t ACCENT = 0x4e59, SELECT = 0x1a89, PANEL = 0x1105;
constexpr int kRows = 5;
constexpr int kWordListTop = 43;
constexpr int kWordListLineStep = 17;  // Previous 15-pixel row step plus 2 pixels.
constexpr int kDefinitionTop = 43;
constexpr int kDefinitionBottom = 121;
constexpr int kDefinitionLineGap = 4;
M5Canvas canvas(&M5Cardputer.Display);
bool spriteReady = false;

class SdReader : public dict::Reader {
public:
    File file;
    uint32_t size() const override { return file ? file.size() : 0; }
    size_t readAt(uint32_t offset, uint8_t* buffer, size_t length) override {
        return file && file.seek(offset) ? file.read(buffer, length) : 0;
    }
};
struct SdLexicon {
    SdReader indexFile, bodyFile;
    dict::StarDict dictionary;
    dict::Metadata meta;
    std::string path;
    bool loaded = false;
    bool cacheHit = false, cacheSaved = false;
    uint32_t loadMs = 0;
    void reset() {
        loaded = false;
        cacheHit = cacheSaved = false; loadMs = 0;
        indexFile.file.close(); bodyFile.file.close();
        dictionary = dict::StarDict();
        path.clear(); meta = {};
    }
};
SdLexicon englishLexicon, chineseLexicon, otherLexicon, pinyinLexicon;
SdLexicon* activeLexicon = nullptr;
dict::StarDict& dictionary() { return activeLexicon->dictionary; }
std::vector<std::string> dictionaryPaths;
std::vector<dict::Entry> matches;
std::vector<size_t> demoMatches;
ime::State composer;
std::string& composition = composer.input;
std::vector<std::string>& candidates = composer.candidates;
int& candidatePage = composer.page;
std::string query, definition, headword;
std::string sourceName = "内置演示", statusMessage;
std::vector<std::string> lines;
int selected = 0, definitionPage = 0, activeDictionary = -1;
bool sdMounted = false, usingSd = false, pinyin = false, dirty = true;
KeyRepeat backspaceRepeat;
bool searchPending = false;
uint32_t searchAt = 0;
uint32_t bootReadyMs = 0;
uint32_t audioNoticeUntil = 0;
bool audioIndexesSuspended = false;
std::string audioRestorePath, wifiSsid, wifiPassword, wifiError;
int wifiField = 0;
enum class Page { Search, Definition, Help, Wifi };
Page page = Page::Search;
Page wifiReturnPage = Page::Search;

M5GFX& screen() { return M5Cardputer.Display; }
bool loadLexicon(SdLexicon& lexicon, const std::string& path, bool rebuild = false);
void switchDirection(bool chinese);
void render();

void printAt(const std::string& text, int x, int y, uint16_t color = FG) {
    canvas.setTextColor(color);
    ui::printText(canvas, text, x, y);
}

int textWidth(const std::string& text) { return ui::textWidth(canvas, text); }

void printScreenAt(std::string text, int x, int y) {
    while (!text.empty() && ui::textWidth(screen(), text) > screen().width() - x - 4) dict::popUtf8(text);
    ui::printText(screen(), text, x, y);
}

std::string fitEnd(std::string text, int width) {
    while (!text.empty() && textWidth(text) > width) dict::popUtf8(text);
    return text;
}

std::string fitTail(std::string text, int width) {
    while (!text.empty() && textWidth(text) > width)
        text.erase(0, dict::utf8Length(uint8_t(text[0])));
    return text;
}

void paintProgress(uint32_t done, uint32_t total) {
    screen().setFont(&kBodyFont);
    screen().setTextColor(FG, BG);
    screen().fillScreen(BG);
    printScreenAt("正在建立词库索引", 8, 30);
    printScreenAt(std::to_string(done) + " / " + std::to_string(total), 8, 55);
    screen().drawRect(8, 85, 224, 8, MUTED);
    screen().fillRect(9, 86, total ? uint64_t(done) * 222 / total : 0, 6, ACCENT);
    delay(1);
}

void updateCandidates() {
    composer.resetLayout();
    candidates.clear();
    if (composition.empty()) return;
    const std::string code = ime::normalize(composition);
    if (code.empty()) return;
    if (!pinyinLexicon.loaded && sdMounted && SD.exists("/dictionaries/pinyin.ifo"))
        loadLexicon(pinyinLexicon, "/dictionaries/pinyin.ifo");
    if (pinyinLexicon.loaded) {
        const auto entries = pinyinLexicon.dictionary.suggest(code, 4);
        const bool exact = !entries.empty() && entries[0].word == code;
        for (const auto& entry : entries) {
            if (!dict::startsWith(entry.word, code) || (exact && entry.word != code)) break;
            bool truncated = false, ok = false;
            const auto text = pinyinLexicon.dictionary.definition(entry, truncated, ok);
            if (ok) ime::addLines(candidates, text, exact ? ime::kMaxCandidates : 3);
        }
    }
    size_t lo = 0, hi = kPinyinCount;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (code.compare(kPinyin[mid].code) > 0) lo = mid + 1; else hi = mid;
    }
    if (lo < kPinyinCount && code == kPinyin[lo].code) {
        const std::string chars = kPinyin[lo].candidates;
        for (size_t pos = 0; pos < chars.size(); pos += dict::utf8Length(uint8_t(chars[pos])))
            ime::addUnique(candidates, chars.substr(pos, dict::utf8Length(uint8_t(chars[pos]))));
    }
    if (!query.empty() && chineseLexicon.loaded) {
        std::vector<std::string> completions;
        for (const auto& entry : chineseLexicon.dictionary.suggest(query, 32)) {
            if (!dict::startsWith(entry.word, query)) break;
            completions.push_back(entry.word);
        }
        std::stable_partition(candidates.begin(), candidates.end(), [&](const std::string& candidate) {
            return std::find(completions.begin(), completions.end(), query + candidate) != completions.end();
        });
    }
    canvas.setFont(&fonts::efontCN_16);
    composer.reflow([](const std::string& text) { return textWidth(text); });
}

void refreshMatches() {
    selected = 0;
    matches.clear(); demoMatches.clear();
    if (query.empty()) { searchPending = false; dirty = true; return; }
    if (usingSd) matches = dictionary().suggest(query, kRows);
    else {
        for (size_t i = 0; i < kDemoCount && demoMatches.size() < kRows; ++i)
            if (dict::compare(kDemo[i].word, query) >= 0) demoMatches.push_back(i);
    }
    searchPending = false;
    dirty = true;
}

void scheduleSearch() {
    searchPending = true;
    searchAt = millis() + 100;
    dirty = true;
}

void clearQuery() {
    query.clear(); composition.clear(); candidates.clear();
    composer.resetLayout();
    page = Page::Search;
    refreshMatches();
}

bool writeIndexCache(const std::string& path, const std::vector<uint8_t>& data) {
    const std::string temporary = path + ".tmp";
    // Only disposable derived cache files are replaced; source dictionaries stay intact.
    if (SD.exists(temporary.c_str()) && !SD.remove(temporary.c_str())) return false;
    File output = SD.open(temporary.c_str(), FILE_WRITE);
    if (!output) return false;
    const bool written = output.write(data.data(), data.size()) == data.size();
    output.flush(); output.close();
    if (!written) { SD.remove(temporary.c_str()); return false; }
    File verify = SD.open(temporary.c_str(), FILE_READ);
    bool ok = verify && verify.size() == data.size();
    uint8_t bytes[1024];
    for (size_t pos = 0; ok && pos < data.size(); pos += sizeof(bytes)) {
        const size_t length = std::min(sizeof(bytes), data.size() - pos);
        ok = verify.read(bytes, length) == length && memcmp(bytes, data.data() + pos, length) == 0;
    }
    verify.close();
    if (!ok) { SD.remove(temporary.c_str()); return false; }
    if (SD.exists(path.c_str()) && !SD.remove(path.c_str())) return false;
    return SD.rename(temporary.c_str(), path.c_str());
}

bool loadLexicon(SdLexicon& lexicon, const std::string& ifoPath, bool rebuild) {
    if (!rebuild && lexicon.loaded && lexicon.path == ifoPath) return true;
    const uint32_t started = millis();
    lexicon.reset();
    File ifo = SD.open(ifoPath.c_str(), FILE_READ);
    if (!ifo || ifo.size() > 16384) { statusMessage = "Cannot read .ifo"; return false; }
    std::string source;
    source.reserve(ifo.size());
    while (ifo.available()) source.push_back(char(ifo.read()));
    const uint32_t ifoModified = uint32_t(ifo.getLastWrite());
    ifo.close();
    dict::Metadata meta;
    std::string error;
    if (!dict::parseMetadata(source, meta, error)) { statusMessage = error; return false; }
    const size_t anchorBytes = ((meta.count + dict::kStride - 1) / dict::kStride) * sizeof(uint32_t);
    const size_t needed = anchorBytes + 16384;
    if (ESP.getFreeHeap() < needed) { statusMessage = "Not enough RAM for dictionary index"; return false; }
    const auto prefix = ifoPath.substr(0, ifoPath.size() - 4);
    lexicon.indexFile.file = SD.open((prefix + ".idx").c_str(), FILE_READ);
    lexicon.bodyFile.file = SD.open((prefix + ".dict").c_str(), FILE_READ);
    if (!lexicon.indexFile.file || !lexicon.bodyFile.file) { statusMessage = "Need .idx and uncompressed .dict"; lexicon.reset(); return false; }
    dict::CacheIdentity identity;
    identity.ifoCrc = ~dict::crc32(reinterpret_cast<const uint8_t*>(source.data()), source.size());
    identity.ifoModified = ifoModified;
    identity.indexModified = uint32_t(lexicon.indexFile.file.getLastWrite());
    identity.bodyModified = uint32_t(lexicon.bodyFile.file.getLastWrite());
    const bool identified = dict::sampleCrc(lexicon.indexFile, identity.indexSampleCrc) &&
                            dict::sampleCrc(lexicon.bodyFile, identity.bodySampleCrc);
    const std::string cachePath = prefix + ".cidx";
    if (!rebuild && identified) {
        SdReader cached;
        cached.file = SD.open(cachePath.c_str(), FILE_READ);
        if (cached.file) {
            lexicon.cacheHit = lexicon.dictionary.restoreIndex(lexicon.indexFile, lexicon.bodyFile, meta, identity, cached, error);
            if (!lexicon.cacheHit) Serial.printf("Index cache rejected %s: %s\n", ifoPath.c_str(), error.c_str());
        }
    }
    uint32_t lastDraw = 0;
    if (!lexicon.cacheHit && !lexicon.dictionary.open(lexicon.indexFile, lexicon.bodyFile, meta, error, [&](uint32_t done, uint32_t total) {
        delay(1);
        if (done == total || millis() - lastDraw >= 250) {
            paintProgress(done, total);
            lastDraw = millis();
        }
    })) {
        statusMessage = error;
        Serial.printf("Dictionary error: %s\n", error.c_str());
        lexicon.reset();
        return false;
    }
    if (!lexicon.cacheHit && identified && heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) > anchorBytes + dict::kCacheHeaderBytes + 4096) {
        const auto serialized = lexicon.dictionary.serializeIndex(identity);
        lexicon.cacheSaved = !serialized.empty() && writeIndexCache(cachePath, serialized);
        if (!lexicon.cacheSaved) Serial.printf("Index cache save failed %s; dictionary usable\n", ifoPath.c_str());
    }
    lexicon.path = ifoPath; lexicon.meta = meta; lexicon.loaded = true;
    lexicon.loadMs = millis() - started;
    Serial.printf("INDEX %s mode=%s ms=%lu bytes=%u saved=%d\n", ifoPath.c_str(), lexicon.cacheHit ? "cache" : "scan",
                  (unsigned long)lexicon.loadMs, unsigned(dict::kCacheHeaderBytes + lexicon.dictionary.indexRamBytes()), lexicon.cacheSaved);
    Serial.printf("Loaded %s: %lu headwords, %u sparse index bytes\n", meta.name.c_str(),
                  (unsigned long)lexicon.dictionary.count(), unsigned(lexicon.dictionary.indexRamBytes()));
    return true;
}

bool loadDictionary(const std::string& path, bool rebuild = false) {
    SdLexicon* slot = path == "/dictionaries/ecdict.ifo" ? &englishLexicon :
                      path == "/dictionaries/cedict.ifo" ? &chineseLexicon : &otherLexicon;
    if (!loadLexicon(*slot, path, rebuild)) return false;
    activeLexicon = slot;
    sourceName = slot->meta.name;
    usingSd = true; statusMessage.clear();
    const auto found = std::find(dictionaryPaths.begin(), dictionaryPaths.end(), path);
    activeDictionary = found == dictionaryPaths.end() ? -1 : int(found - dictionaryPaths.begin());
    return true;
}

void switchDirection(bool chinese) {
    pinyin = chinese;
    const std::string path = chinese ? "/dictionaries/cedict.ifo" : "/dictionaries/ecdict.ifo";
    if (!sdMounted || !SD.exists(path.c_str()) || !loadDictionary(path)) {
        usingSd = false; activeLexicon = nullptr; activeDictionary = -1; sourceName = "内置演示";
        statusMessage = chinese ? "未安装 cedict 汉英词库" : "未安装 ecdict 英汉词库";
    }
    if (chinese && sdMounted && SD.exists("/dictionaries/pinyin.ifo")) {
        if (!loadLexicon(pinyinLexicon, "/dictionaries/pinyin.ifo"))
            Serial.printf("IME fallback: %s\n", statusMessage.c_str());
    }
    clearQuery();
}

void scanSd(bool rebuild = false) {
    usingSd = false;
    activeLexicon = nullptr;
    englishLexicon.reset(); chineseLexicon.reset(); otherLexicon.reset(); pinyinLexicon.reset();
    pinyin = false;
    dictionaryPaths.clear(); activeDictionary = -1;
    if (sdMounted) SD.end();
    SPI.begin(40, 39, 14, 12);
    sdMounted = false;
    // Some cards need an extra initialization attempt after a USB warm reset.
    for (const uint32_t frequency : {10000000u, 4000000u, 1000000u}) {
        delay(100);
        if (SD.begin(12, SPI, frequency, "/sd", 16) && SD.cardType() != CARD_NONE) {
            sdMounted = true;
            break;
        }
        SD.end();
    }
    if (!sdMounted) {
        sourceName = "内置演示"; statusMessage = "SD 未就绪，使用演示词库";
    } else {
        File dir = SD.open("/dictionaries");
        if (dir && dir.isDirectory()) {
            for (File item = dir.openNextFile(); item; item = dir.openNextFile()) {
                const std::string name = item.name();
                if (!item.isDirectory() && name.size() > 4 && name.substr(name.size() - 4) == ".ifo") {
                    const auto slash = name.find_last_of('/');
                    const auto base = slash == std::string::npos ? name : name.substr(slash + 1);
                    if (base.rfind("._", 0) != 0 && base != "pinyin.ifo") dictionaryPaths.push_back("/dictionaries/" + base);
                }
                item.close();
                if (dictionaryPaths.size() == 16) break;
            }
        }
        dir.close();
        std::sort(dictionaryPaths.begin(), dictionaryPaths.end());
        // Prefer the useful full dictionary over the bundled demonstration.
        auto preferred = std::find(dictionaryPaths.begin(), dictionaryPaths.end(), "/dictionaries/ecdict.ifo");
        if (preferred != dictionaryPaths.end()) std::rotate(dictionaryPaths.begin(), preferred, preferred + 1);
        for (size_t i = 0; i < dictionaryPaths.size(); ++i) {
            if (loadDictionary(dictionaryPaths[i], rebuild)) { pinyin = activeLexicon == &chineseLexicon; break; }
        }
        if (!usingSd) {
            sourceName = "内置演示";
            if (statusMessage.empty()) statusMessage = "SD 内未找到词库";
        }
    }
    if (rebuild && sdMounted) {
        if (SD.exists("/dictionaries/cedict.ifo")) loadLexicon(chineseLexicon, "/dictionaries/cedict.ifo", true);
        if (SD.exists("/dictionaries/pinyin.ifo")) loadLexicon(pinyinLexicon, "/dictionaries/pinyin.ifo", true);
    }
    clearQuery();
}

void nextDictionary() {
    if (dictionaryPaths.empty()) { scanSd(); return; }
    int next = activeDictionary + 1;
    if (next >= int(dictionaryPaths.size())) {
        usingSd = false; activeDictionary = -1; sourceName = "内置演示";
        activeLexicon = nullptr; statusMessage.clear(); pinyin = false;
    } else {
        activeDictionary = next;
        if (!loadDictionary(dictionaryPaths[next])) {
            usingSd = false; sourceName = "内置演示";
        } else pinyin = activeLexicon == &chineseLexicon;
        if (pinyin && SD.exists("/dictionaries/pinyin.ifo")) loadLexicon(pinyinLexicon, "/dictionaries/pinyin.ifo");
    }
    clearQuery();
}

int bodyFontHeight() {
    lgfx::FontMetrics metrics = {};
    kBodyFont.getDefaultMetric(&metrics);
    return std::max(1, int(metrics.height));
}

int definitionLineStep() { return bodyFontHeight() + kDefinitionLineGap; }

int definitionRows() {
    return std::max(1, (kDefinitionBottom - kDefinitionTop + kDefinitionLineGap) / definitionLineStep());
}

int definitionPageCount() {
    return std::max(1, int((lines.size() + definitionRows() - 1) / definitionRows()));
}

int wordListRows() {
    const int bottom = composition.empty() ? 121 : 88;
    return std::min(kRows, std::max(1, (bottom - kWordListTop + kWordListLineStep - bodyFontHeight()) / kWordListLineStep));
}

int wordListStart() {
    const int total = usingSd ? int(matches.size()) : int(demoMatches.size());
    const int rows = wordListRows();
    return std::max(0, std::min(selected - rows + 1, std::max(0, total - rows)));
}

void wrapDefinition() {
    lines.clear();
    canvas.setFont(&kBodyFont);
    std::string current;
    for (size_t pos = 0; pos < definition.size();) {
        const uint8_t c = uint8_t(definition[pos]);
        if (c == '\r') { ++pos; continue; }
        if (c == '\n') { lines.push_back(current); current.clear(); ++pos; continue; }
        const size_t bytes = std::min(dict::utf8Length(c), definition.size() - pos);
        const auto glyph = c == '\t' ? std::string(" ") : definition.substr(pos, bytes);
        pos += bytes;
        if (!current.empty() && textWidth(current + glyph) > 228) {
            lines.push_back(current); current.clear();
        }
        current += glyph;
    }
    if (!current.empty() || lines.empty()) lines.push_back(current);
    definitionPage = 0;
}

void openSelected() {
    if (searchPending) refreshMatches();
    if (usingSd) {
        if (matches.empty() || selected >= int(matches.size())) return;
        const auto& entry = matches[selected];
        headword = entry.word;
        bool truncated = false, ok = false;
        definition = dictionary().definition(entry, truncated, ok);
        if (!ok) definition = "读取失败。请检查 SD 卡，并按 Fn+R 重新加载。";
        if (truncated) definition += "\n[释义过长，已截断]";
    } else {
        if (demoMatches.empty() || selected >= int(demoMatches.size())) return;
        const auto& entry = kDemo[demoMatches[selected]];
        headword = entry.word; definition = entry.text;
    }
    wrapDefinition();
    page = Page::Definition; dirty = true;
    Serial.printf("LOOKUP: %s\n", headword.c_str());
}

void chooseCandidate(int number) {
    if (composer.select(number, query)) scheduleSearch();
}

void prepareAudioNetwork() {
    if (audioIndexesSuspended) return;
    audioRestorePath = usingSd && activeLexicon ? activeLexicon->path : "";
    audioIndexesSuspended = true;
    activeLexicon = nullptr; usingSd = false;
    englishLexicon.reset(); chineseLexicon.reset(); pinyinLexicon.reset(); otherLexicon.reset();
}

void restoreAfterAudioNetwork() {
    if (!audioIndexesSuspended || pronunciation::busy()) return;
    audioIndexesSuspended = false;
    const int oldSelected = selected;
    if (!audioRestorePath.empty()) loadDictionary(audioRestorePath);
    audioRestorePath.clear();
    refreshMatches();
    const int count = usingSd ? int(matches.size()) : int(demoMatches.size());
    selected = std::max(0, std::min(oldSelected, std::max(0, count - 1)));
    dirty = true;
}

void openWifiSettings() {
    pronunciation::stop(); restoreAfterAudioNetwork();
    wifiReturnPage = page == Page::Wifi ? Page::Search : page;
    wifiSsid = pronunciation::savedSsid(); wifiPassword.clear(); wifiError.clear(); wifiField = 0;
    page = Page::Wifi; dirty = true;
}

void pronounceSelected() {
    if (pronunciation::busy()) { pronunciation::stop(); dirty = true; return; }
    if (page == Page::Help || page == Page::Wifi || !composition.empty()) return;
    std::string value;
    if (page == Page::Definition) value = headword;
    else {
        if (searchPending) refreshMatches();
        if (usingSd && selected < int(matches.size())) value = matches[selected].word;
        else if (!usingSd && selected < int(demoMatches.size())) value = kDemo[demoMatches[selected]].word;
    }
    pronunciation::request(value, sdMounted);
    dirty = true;
}

void move(int amount) {
    if (page == Page::Definition) {
        const int pages = definitionPageCount();
        definitionPage = (definitionPage + amount % pages + pages) % pages;
    } else if (page == Page::Search) {
        if (searchPending) refreshMatches();
        const int total = usingSd ? matches.size() : demoMatches.size();
        selected = std::max(0, std::min(std::max(0, total - 1), selected + amount));
    }
    dirty = true;
}

void handleKey(int x, int y, const Keyboard_Class::KeysState& state) {
    const auto key = M5Cardputer.Keyboard.getKeyValue({x, y});
    const char base = key.value_first;
    if (pronunciation::busy()) {
        if (base == '`' || (state.fn && base == 'p')) pronunciation::stop();
        else if (state.fn && base == '=') pronunciation::volume(16);
        else if (state.fn && base == '-') pronunciation::volume(-16);
        dirty = true; return;
    }
    if (page == Page::Wifi) {
        if (base == '`') { wifiPassword.clear(); page = wifiReturnPage; dirty = true; return; }
        if (x == 0 && y == 1) { wifiField = 1 - wifiField; dirty = true; return; }
        auto& field = wifiField ? wifiPassword : wifiSsid;
        if (x == 13 && y == 0) { dict::popUtf8(field); dirty = true; return; }
        if (x == 13 && y == 2) {
            if (wifiField == 0) wifiField = 1;
            else if (pronunciation::saveWifi(wifiSsid, wifiPassword)) {
                wifiPassword.clear(); page = wifiReturnPage;
            } else wifiError = "SSID不能为空，密码至少8位";
            dirty = true; return;
        }
        if (!state.fn && !state.ctrl && !state.alt && uint8_t(base) >= 32 && uint8_t(base) < 127) {
            const char c = state.shift ? key.value_second : base;
            if (field.size() < (wifiField ? 63u : 32u)) field += c;
            wifiError.clear(); dirty = true;
        }
        return;
    }
    if (state.fn) {
        if (base == 'h') { page = page == Page::Help ? Page::Search : Page::Help; dirty = true; }
        else if (base == 'r') scanSd(state.shift);
        else if (base == 'd') nextDictionary();
        else if (base == 'p') pronounceSelected();
        else if (base == 'w') openWifiSettings();
        else if (base == '=') pronunciation::volume(16);
        else if (base == '-') pronunciation::volume(-16);
        else if (x == 13 && y == 0) clearQuery();
        else if (base == ';') move(-1);
        else if (base == '.') move(1);
        else if (base == ',') move(-1);
        else if (base == '/') move(1);
        return;
    }
    if (x == 0 && y == 1) {
        switchDirection(!pinyin); return;
    }
    if (base == '`') {
        if (page != Page::Search) page = Page::Search;
        else if (!composition.empty()) { composition.clear(); candidates.clear(); composer.resetLayout(); }
        else clearQuery();
        dirty = true; return;
    }
    if (x == 13 && y == 0) {
        if (page != Page::Search) page = Page::Search;
        else if (!composition.empty()) { composition.pop_back(); updateCandidates(); }
        else { dict::popUtf8(query); scheduleSearch(); }
        dirty = true; return;
    }
    if (x == 13 && y == 2) {
        if (page == Page::Help) page = Page::Search;
        else if (page == Page::Definition) move(1);
        else if (!composition.empty()) chooseCandidate(0);
        else openSelected();
        dirty = true; return;
    }
    if (uint8_t(base) >= 0x80 || base == 0 || state.ctrl || state.alt || state.opt) return;
    if (page == Page::Help) return;
    if (page == Page::Definition && base == ' ') { move(1); return; }
    if (page != Page::Search) { page = Page::Search; query.clear(); }
    const char c = state.shift ? key.value_second : base;
    if (pinyin && !composition.empty()) {
        if (c >= '1' && c <= '9') { chooseCandidate(c - '1'); return; }
        if (c >= '0' && c <= '9') return;
        if (c == ' ') { chooseCandidate(0); return; }
        if (c == '[' || c == ']') {
            composer.turnPage(c == ']' ? 1 : -1);
            dirty = true; return;
        }
    }
    if (pinyin && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'')) {
        composer.append(c);
        updateCandidates(); dirty = true;
    } else if (c >= 32 && c <= 126 && query.size() < 120) {
        query += c; scheduleSearch();
    }
}

Keyboard_Class::KeysState keyState(uint64_t held) {
    Keyboard_Class::KeysState result;
    const uint8_t flags = keyboard::modifiers(held);
    result.fn = flags & keyboard::kFn; result.shift = flags & keyboard::kShift;
    result.ctrl = flags & keyboard::kCtrl; result.opt = flags & keyboard::kOpt; result.alt = flags & keyboard::kAlt;
    return result;
}

void handleKeyEvent(const keyboard::Event& event) {
    auto state = keyState(event.held);
    const bool backspace = event.key == 13;
    const bool plain = !state.fn && !state.ctrl && !state.alt && !state.opt;
    const bool heldBackspace = event.held & keyboard::keyBit(13, 0);
    if (!plain || event.audio || pronunciation::busy()) backspaceRepeat.update(millis(), heldBackspace, false);
    if (backspace && !event.pressed) { backspaceRepeat.update(millis(), false); return; }
    // Do not replay typing captured while a sound/network operation blocked it.
    if (event.audio && !pronunciation::busy()) return;
    if (!event.pressed) return;
    if (backspace && plain) {
        if (backspaceRepeat.update(millis(), true, !pronunciation::busy())) handleKey(13, 0, state);
    } else handleKey(event.key % 14, event.key / 14, state);
}

void render() {
    canvas.fillSprite(BG);
    canvas.setTextWrap(false);
    canvas.setFont(&fonts::efontCN_12);
    canvas.fillRect(0, 0, 240, 18, PANEL);
    printAt("CARDPUTER DICT", 5, 2, ACCENT);
    const std::string direction = pinyin ? "汉英" : "英汉";
    const std::string shortcut = "[Tab]";
    const int directionX = canvas.width() - 5 - textWidth(direction);
    printAt(shortcut, directionX - 5 - textWidth(shortcut), 2, MUTED);
    printAt(direction, directionX, 2, ACCENT);
    if (pronunciation::busy()) {
        canvas.setFont(&kBodyFont);
        printAt(fitEnd(pronunciation::word(), 228), 5, 25, ACCENT);
        printAt(fitEnd(pronunciation::message(), 228), 5, 49);
        printAt("Fn+P 或 ` 停止", 5, 77, MUTED);
        printAt("Fn+= / Fn+- 调整音量", 5, 96, MUTED);
        canvas.setFont(&fonts::efontCN_12);
        printAt("音量 " + std::to_string(pronunciation::volume() * 100 / 255) + "%", 5, 121, MUTED);
    } else if (page == Page::Wifi) {
        canvas.setFont(&kBodyFont);
        printAt("WiFi设置（2.4GHz）", 5, 22, ACCENT);
        printAt((wifiField == 0 ? "> " : "  ") + std::string("SSID: ") + fitTail(wifiSsid, 150), 5, 43);
        printAt((wifiField == 1 ? "> " : "  ") + std::string("密码: ") + fitTail(std::string(wifiPassword.size(), '*'), 150), 5, 62);
        printAt("Tab换字段 Enter下一步/保存", 5, 82, MUTED);
        canvas.setFont(&fonts::efontCN_12);
        printAt("联网获取将发送所选英文词", 5, 102, MUTED);
        printAt(wifiError.empty() ? "`取消，空密码连接开放网络" : fitEnd(wifiError, 230), 5, 120, wifiError.empty() ? MUTED : FG);
    } else if (page == Page::Help) {
        canvas.setFont(&kBodyFont);
        const char* help[] = {"Tab 切换  Enter 查词", "Fn+; / Fn+. 上下/翻页", "空格首选  1-9 选词", "[ / ] 翻候选  v=ü", "Fn+D 换词库  Fn+R 重载", "Fn+退格 清空  ` 返回", "Fn+P 朗读  Fn+W WiFi"};
        for (size_t i = 0; i < 7; ++i) printAt(fitEnd(help[i], 232), 4, 23 + int(i) * 16);
    } else if (page == Page::Definition) {
        canvas.setFont(&fonts::efontCN_16);
        printAt(fitEnd(headword, 228), 5, 21, ACCENT);
        canvas.drawFastHLine(4, 40, 232, SELECT);
        canvas.setFont(&kBodyFont);
        const int rows = definitionRows();
        for (int row = 0; row < rows; ++row) {
            const size_t i = size_t(definitionPage * rows + row);
            if (i < lines.size()) printAt(lines[i], 5, kDefinitionTop + row * definitionLineStep());
        }
        canvas.setFont(&fonts::efontCN_12);
        const std::string footer = std::to_string(definitionPage + 1) + "/" + std::to_string(definitionPageCount()) + " Space下一页 Fn+P朗读";
        printAt(footer, 5, 121, MUTED);
    } else {
        canvas.setFont(&fonts::efontCN_16);
        printAt(query.empty() ? (pinyin ? "输入拼音…" : "输入单词…") : fitTail(query, 214), 5, 21, query.empty() ? MUTED : FG);
        canvas.drawFastHLine(4, 40, 232, SELECT);
        canvas.setFont(&kBodyFont);
        const size_t count = usingSd ? matches.size() : demoMatches.size();
        const size_t start = wordListStart();
        const size_t end = std::min(count, start + wordListRows());
        for (size_t index = start; index < end; ++index) {
            const int y = kWordListTop + int(index - start) * kWordListLineStep;
            const auto word = usingSd ? matches[index].word : std::string(kDemo[demoMatches[index]].word);
            if (int(index) == selected) canvas.fillRoundRect(3, y - 1, 234, 15, 2, SELECT);
            printAt(fitEnd(word, 222), 7, y);
        }
        if (query.empty()) {
            printAt(pinyin ? "全拼输入，空格或数字选词" : "输入英文后按Enter查询", 5, 45, MUTED);
            if (pinyin) printAt("选词后按Enter查询释义", 5, 63, MUTED);
        } else if (count == 0) printAt("未找到；可换词库或缩短输入", 5, 45, MUTED);
        if (!composition.empty()) {
            canvas.fillRect(0, 88, 240, 33, PANEL);
            canvas.setFont(&fonts::efontCN_12);
            printAt(fitTail(composition, 170), 4, 88, ACCENT);
            printAt(std::to_string(candidatePage + 1) + "/" + std::to_string(composer.pageCount()), 196, 88, ACCENT);
            canvas.setFont(&fonts::efontCN_16);
            for (size_t i = 0; i < composer.visibleCount(); ++i) {
                const size_t index = composer.pageStart() + i;
                if (index >= candidates.size() || index >= composer.placements.size()) break;
                const auto& word = candidates[index];
                const auto& position = composer.placements[index];
                auto label = fitEnd(word, position.wordWidth);
                if (label != word) {
                    while (!label.empty() && textWidth(label + "…") > position.wordWidth) dict::popUtf8(label);
                    label += "…";
                }
                const int x = 4 + position.x, y = 102;
                printAt(std::to_string(i + 1), x, y, ACCENT);
                printAt(label, x + position.labelWidth + 1, y);
            }
            if (candidates.empty()) printAt("无候选，请检查拼音", 4, 102);
        }
        canvas.setFont(&fonts::efontCN_12);
        const std::string footer = usingSd ? "SD " + std::to_string(dictionary().count()) + "词  Fn+H帮助" : "演示 " + std::to_string(kDemoCount) + "词  Fn+H帮助";
        printAt(footer, 4, 121, MUTED);
    }
    if (!pronunciation::busy() && page != Page::Wifi && !pronunciation::message().empty() && int32_t(audioNoticeUntil - millis()) > 0) {
        canvas.fillRect(0, 118, 240, 17, PANEL);
        canvas.setFont(&fonts::efontCN_12);
        printAt(fitEnd(pronunciation::message(), 230), 4, 120, ACCENT);
    }
    canvas.pushSprite(0, 0);
    dirty = false;
}

uint32_t fnvUpdate(uint32_t hash, const uint8_t* bytes, size_t length) {
    for (size_t i = 0; i < length; ++i) hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

void receiveFile(const std::string& request) {
    char name[64] = {};
    unsigned long length = 0, expectedHash = 0;
    const bool audioUpload = request.rfind("putaudio ", 0) == 0;
    if (sscanf(request.c_str(), audioUpload ? "putaudio %63s %lu %lx" : "put %63s %lu %lx", name, &length, &expectedHash) != 3 ||
        !length || length > 64 * 1024 * 1024 || !sdMounted) {
        Serial.println("ERR invalid request or SD unavailable"); return;
    }
    const std::string base = name;
    if (base.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-%") != std::string::npos ||
        base[0] == '.' || base.find("..") != std::string::npos) {
        Serial.println("ERR invalid filename"); return;
    }
    const auto dot = base.find_last_of('.');
    if (audioUpload ? (dot == std::string::npos || (base.substr(dot) != ".mp3" && base.substr(dot) != ".wav") || length > 512 * 1024) :
        (dot == std::string::npos || (base.substr(dot) != ".ifo" && base.substr(dot) != ".idx" && base.substr(dot) != ".dict" &&
        base.substr(dot) != ".txt" && base.substr(dot) != ".json"))) {
        Serial.println("ERR only dictionary data and license files allowed"); return;
    }
    const std::string directory = audioUpload ? "/audio/en" : "/dictionaries";
    const std::string target = directory + "/" + base;
    const std::string partial = directory + "/." + base + ".part";
    // Keep this out of serialCommand's stack frame when the compiler inlines
    // receiveFile; normal commands can invoke nested index-cache/file routines.
    static uint8_t buffer[4096];
    if (SD.exists(target.c_str())) {
        File existing = SD.open(target.c_str(), FILE_READ);
        uint32_t hash = 2166136261u;
        if (!existing || existing.size() != length) { Serial.println("ERR existing file differs; preserved"); return; }
        while (existing.available()) {
            const size_t n = existing.read(buffer, sizeof(buffer));
            if (!n) { Serial.println("ERR existing file read failed"); return; }
            hash = fnvUpdate(hash, buffer, n); delay(1);
        }
        existing.close();
        Serial.println(hash == uint32_t(expectedHash) ? "SKIP verified identical" : "ERR existing file differs; preserved");
        return;
    }
    if (SD.exists(partial.c_str())) { Serial.println("ERR partial file exists; remove it before retrying"); return; }
    if (SD.totalBytes() - SD.usedBytes() < uint64_t(length) + 65536) { Serial.println("ERR insufficient SD space"); return; }
    if (audioUpload && !SD.exists("/audio")) SD.mkdir("/audio");
    if (!SD.exists(directory.c_str()) && !SD.mkdir(directory.c_str())) { Serial.println("ERR mkdir failed"); return; }
    File output = SD.open(partial.c_str(), FILE_WRITE);
    if (!output) { Serial.println("ERR cannot create partial file"); return; }
    Serial.println("READY 4096");
    uint32_t received = 0, hash = 2166136261u, lastDraw = 0;
    while (received < length) {
        const size_t chunk = std::min(sizeof(buffer), size_t(length - received));
        size_t available = 0;
        const uint32_t started = millis();
        uint32_t lastAck = started;
        while (available < chunk && millis() - started < 60000) {
            const int n = Serial.read(buffer + available, chunk - available);
            if (n > 0) available += size_t(n); else delay(1);
            // Repeat the cumulative acknowledgement if one was lost by the
            // host's USB stack. The host ignores acknowledgements already seen.
            if (!available && millis() - lastAck > 1000) {
                Serial.printf("ACK %lu\n", (unsigned long)received);
                Serial.flush();
                lastAck = millis();
            }
        }
        if (available != chunk || output.write(buffer, chunk) != chunk) {
            output.close(); SD.remove(partial.c_str());
            Serial.printf("ERR transfer incomplete %u/%u bytes\n", unsigned(available), unsigned(chunk));
            dirty = true; return;
        }
        hash = fnvUpdate(hash, buffer, chunk);
        received += chunk;
        Serial.printf("ACK %lu\n", (unsigned long)received);
        Serial.flush();
        if (millis() - lastDraw > 500) {
            screen().fillScreen(BG); screen().setFont(&kBodyFont);
            screen().setTextColor(FG);
            printScreenAt(audioUpload ? "USB 音频传输中" : "USB 词库传输中", 6, 30);
            printScreenAt(name, 6, 55);
            printScreenAt(std::to_string(received / 1024) + " / " + std::to_string(length / 1024) + " KB", 6, 80);
            lastDraw = millis();
        }
        delay(1);
    }
    output.flush(); output.close();
    // Verify bytes read back from the card before committing the new filename.
    File verify = SD.open(partial.c_str(), FILE_READ);
    uint32_t savedHash = 2166136261u, savedBytes = 0;
    while (verify && verify.available()) {
        const size_t n = verify.read(buffer, sizeof(buffer));
        if (!n) break;
        savedHash = fnvUpdate(savedHash, buffer, n); savedBytes += n; delay(1);
    }
    verify.close();
    if (hash != uint32_t(expectedHash) || savedHash != hash || savedBytes != length) {
        SD.remove(partial.c_str()); Serial.println("ERR checksum mismatch");
    } else if (!SD.rename(partial.c_str(), target.c_str())) Serial.println("ERR rename failed; partial retained");
    else {
        // A new source file invalidates its derived cache even if FAT timestamps match.
        if (base.substr(dot) == ".ifo" || base.substr(dot) == ".idx" || base.substr(dot) == ".dict") {
            const std::string derived = "/dictionaries/" + base.substr(0, dot) + ".cidx";
            if (SD.exists(derived.c_str())) SD.remove(derived.c_str());
        }
        Serial.println("DONE verified");
    }
    dirty = true;
}

bool injectCharacter(char c) {
    Keyboard_Class::KeysState state;
    for (int y = 0; y < 4; ++y) for (int x = 0; x < 14; ++x) {
        const auto key = M5Cardputer.Keyboard.getKeyValue({x, y});
        if (key.value_first == c || key.value_second == c) {
            state.shift = key.value_first != c;
            handleKey(x, y, state);
            return true;
        }
    }
    return false;
}

void printInputState() {
    Serial.printf("STATE direction=%s query=%s composition=%s page=%d candidate_page=%d visible=%u candidate_start=%u selected=%d list_start=%d list_rows=%d\n",
                  pinyin ? "zh-en" : "en-zh", query.c_str(), composition.c_str(), int(page), candidatePage,
                  unsigned(composer.visibleCount()), unsigned(composer.pageStart()), selected, wordListStart(), wordListRows());
    if (page == Page::Definition) Serial.printf("TEXT: %s\n", definition.c_str());
    if (page == Page::Definition) Serial.printf("DEFINITION page=%d pages=%d\n", definitionPage, definitionPageCount());
}

void serialCommand() {
    static std::string command;
    static bool overflow = false;
    while (Serial.available()) {
        const char c = char(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            if (overflow) Serial.println("ERR command too long");
            else if (pronunciation::busy() && command != "status" && command != "audio status" && command != "audio stop" && command != "screenshot" && command != "press pronounce")
                Serial.println("ERR audio busy; use audio stop");
            else if (command.rfind("put ", 0) == 0 || command.rfind("putaudio ", 0) == 0) receiveFile(command);
            else if (command == "status") {
                Serial.printf("CardputerDict 0.4.2 board=%d heap=%u sd=%d words=%lu source=%s direction=%s\n", int(M5.getBoard()), ESP.getFreeHeap(), sdMounted,
                              (unsigned long)(usingSd ? dictionary().count() : kDemoCount), sourceName.c_str(), pinyin ? "zh-en" : "en-zh");
                Serial.printf("display=%dx%d rotation=%u\n", screen().width(), screen().height(), unsigned(screen().getRotation()));
                Serial.printf("boot_ready_ms=%lu\n", (unsigned long)bootReadyMs);
                keyboard::diagnostics();
                Serial.println("spacing half=1 full=2");
                Serial.printf("definition font_height=%d line_gap=%d line_step=%d rows=%d\n", bodyFontHeight(), kDefinitionLineGap, definitionLineStep(), definitionRows());
                Serial.printf("results line_step=%d rows=%d start=%d selected=%d count=%u\n", kWordListLineStep, wordListRows(), wordListStart(), selected, unsigned(usingSd ? matches.size() : demoMatches.size()));
                Serial.printf("lexicons en=%lu zh=%lu ime=%lu\n", (unsigned long)(englishLexicon.loaded ? englishLexicon.dictionary.count() : 0),
                              (unsigned long)(chineseLexicon.loaded ? chineseLexicon.dictionary.count() : 0),
                              (unsigned long)(pinyinLexicon.loaded ? pinyinLexicon.dictionary.count() : 0));
                for (const auto* lexicon : {&englishLexicon, &chineseLexicon, &pinyinLexicon}) {
                    if (lexicon->loaded) Serial.printf("cache path=%s mode=%s load_ms=%lu saved=%d\n", lexicon->path.c_str(), lexicon->cacheHit ? "cache" : "scan",
                        (unsigned long)lexicon->loadMs, lexicon->cacheSaved);
                }
                Serial.printf("note=%s\n", statusMessage.c_str());
                pronunciation::diagnostics();
            } else if (command == "audio status") pronunciation::diagnostics();
            else if (command == "audio stop") pronunciation::stop();
            else if (command == "wifi") openWifiSettings();
            else if (command.rfind("speak ", 0) == 0) {
                pronunciation::request(command.substr(6), sdMounted); pronunciation::diagnostics();
            } else if (command.rfind("speaklocal ", 0) == 0) {
                pronunciation::request(command.substr(11), sdMounted, false); pronunciation::diagnostics();
            } else if (command == "reload") scanSd();
            else if (command == "reindex") scanSd(true);
            else if (command == "restart") { Serial.println("RESTARTING"); Serial.flush(); delay(10); ESP.restart(); }
            else if (command == "screenshot") {
                if (dirty) render();
                Serial.print("PPM_BEGIN\nP6\n240 135\n255\n");
                uint8_t pixels[240 * 3];
                for (int y = 0; y < 135; ++y) {
                    canvas.readRectRGB(0, y, 240, 1, pixels);
                    Serial.write(pixels, sizeof(pixels));
                }
                Serial.print("\nPPM_END\n");
            } else if (command.rfind("ime ", 0) == 0 && command.size() <= 4 + ime::kMaxComposition) {
                switchDirection(true);
                composition = command.substr(4);
                page = Page::Search; updateCandidates(); dirty = true;
                Serial.print("CANDIDATES:");
                for (const auto& word : candidates) { Serial.print(" "); Serial.print(word.c_str()); }
                Serial.println();
            } else if (command.rfind("type ", 0) == 0) {
                for (char c : command.substr(5)) {
                    if (uint8_t(c) < 32 || uint8_t(c) > 126 || !injectCharacter(c)) break;
                }
                printInputState();
            } else if (command.rfind("keytest ", 0) == 0) {
                const auto text = command.substr(8);
                uint8_t keys[32]; size_t count = 0;
                for (const auto c : text) {
                    bool found = false;
                    for (int y = 0; y < 4 && !found; ++y) for (int x = 0; x < 14 && !found; ++x) {
                        if (M5Cardputer.Keyboard.getKeyValue({x, y}).value_first == c && count < sizeof(keys)) {
                            keys[count++] = uint8_t(y * 14 + x); found = true;
                        }
                    }
                    if (!found) { count = 0; break; }
                }
                if (page != Page::Search || composition.size() || !count || !keyboard::injectTaps(keys, count)) Serial.println("ERR keytest needs a search input and 1..32 supported characters");
                else Serial.println("KEYTEST held_ms=8 gap_ms=4");
            } else if (command == "keytest-hold-backspace") {
                const uint8_t key = 13;
                Serial.println(page == Page::Search && keyboard::injectTaps(&key, 1, 1200, 4) ? "KEYTEST backspace held_ms=1200" : "ERR keytest busy");
            } else if (command == "input state") {
                printInputState();
            } else if (command.rfind("press ", 0) == 0) {
                const auto key = command.substr(6);
                Keyboard_Class::KeysState state;
                if (key == "tab") handleKey(0, 1, state);
                else if (key == "enter") handleKey(13, 2, state);
                else if (key == "backspace") handleKey(13, 0, state);
                else if (key == "space") injectCharacter(' ');
                else if (key == "clear") { state.fn = true; handleKey(13, 0, state); }
                else if (key == "down") { state.fn = true; handleKey(11, 3, state); }
                else if (key == "up") { state.fn = true; handleKey(11, 2, state); }
                else if (key == "previous") { state.fn = true; handleKey(10, 3, state); }
                else if (key == "next") { state.fn = true; handleKey(12, 3, state); }
                else if (key == "pronounce") { state.fn = true; handleKey(10, 1, state); }
                else if (key.size() == 1) injectCharacter(key[0]);
                else Serial.println("ERR unknown key");
                printInputState();
            }
            else if (command.rfind("lookup ", 0) == 0) {
                const std::string word = command.substr(7);
                switchDirection(ime::hasHan(word));
                query = word;
                refreshMatches();
                const std::string first = usingSd ? (matches.empty() ? "" : matches[0].word) : (demoMatches.empty() ? "" : kDemo[demoMatches[0]].word);
                if (dict::compare(first, query) != 0 || first.empty()) Serial.println("NOT_FOUND");
                else { openSelected(); Serial.println(definition.c_str()); }
            } else if (!command.empty()) Serial.println("Commands: status | lookup WORD | reload | reindex | ime PINYIN | screenshot | type TEXT | press KEY");
            if (command != "screenshot" && command.rfind("put ", 0) != 0 && command.rfind("putaudio ", 0) != 0) Serial.println("CMD_DONE");
            command.clear(); overflow = false;
        } else if (command.size() < 128) command += c;
        else overflow = true;
    }
}
} // namespace

void setup() {
    // The ESP32-S3 HW CDC default is only 256 bytes; uploads use 4 KB blocks.
    Serial.setRxBufferSize(8192);
    Serial.setTxBufferSize(4096);
    Serial.setTxTimeoutMs(2000);
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.internal_spk = true;
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg, true);
    pronunciation::begin();
    pronunciation::setPrepareNetwork(prepareAudioNetwork);
    screen().setRotation(0);  // M5GFX includes Cardputer's panel rotation offset.
    screen().setBrightness(150);
    canvas.setColorDepth(16);
    spriteReady = canvas.createSprite(screen().width(), screen().height()) != nullptr;
    if (!spriteReady) {
        screen().fillScreen(BG); screen().setTextColor(FG);
        screen().setFont(&kBodyFont);
        printScreenAt("Not enough RAM", 6, 30);
        printScreenAt("for screen buffer", 6, 50);
        return;
    }
    scanSd();
    query = "apple";
    refreshMatches();
    render();
    if (!keyboard::begin()) {
        screen().fillScreen(BG); screen().setFont(&kBodyFont); screen().setTextColor(FG);
        printScreenAt("Keyboard startup failed", 5, 30);
        spriteReady = false; return;
    }
    bootReadyMs = millis();
    Serial.printf("BOOT_READY ms=%lu\n", (unsigned long)bootReadyMs);
    Serial.println("CardputerDict ready. Commands: status, lookup apple, reload");
}

void loop() {
    if (!spriteReady) { delay(100); return; }
    M5.update();
    keyboard::Event event;
    for (int count = 0; count < 96 && keyboard::next(event); ++count) handleKeyEvent(event);
    const uint64_t held = keyboard::heldKeys();
    const auto state = keyState(held);
    const bool allowBackspace = !state.fn && !state.ctrl && !state.alt && !state.opt && !pronunciation::busy();
    if (backspaceRepeat.update(millis(), held & keyboard::keyBit(13, 0), allowBackspace)) handleKey(13, 0, state);
    pronunciation::tick();
    restoreAfterAudioNetwork();
    if (pronunciation::changed()) { audioNoticeUntil = millis() + 2500; dirty = true; }
    if (audioNoticeUntil && int32_t(millis() - audioNoticeUntil) >= 0) { audioNoticeUntil = 0; dirty = true; }
    // Repeated deletion must not wait for an SD lookup between every repeat.
    // Keep the input responsive, then refresh matches after physical release.
    if (searchPending && !(keyboard::heldKeys() & keyboard::keyBit(13, 0)) && int32_t(millis() - searchAt) >= 0) refreshMatches();
    if (M5Cardputer.BtnA.wasPressed() && !pronunciation::busy() && page != Page::Wifi) { page = page == Page::Help ? Page::Search : Page::Help; dirty = true; }
    serialCommand();
    if (dirty) render();
    delay(1);
}
