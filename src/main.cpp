#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "dict_core.h"
#include "demo_data.h"
#include "pinyin_data.h"
#include "body_font.h"

namespace {
constexpr uint16_t BG = 0x0863, FG = 0xef7d, MUTED = 0x8c92;
constexpr uint16_t ACCENT = 0x4e59, SELECT = 0x1a89, PANEL = 0x1105;
constexpr int kRows = 5;
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
SdReader indexFile, bodyFile;
dict::StarDict dictionary;
std::vector<std::string> dictionaryPaths;
std::vector<dict::Entry> matches;
std::vector<size_t> demoMatches;
std::string query, composition, candidates, definition, headword;
std::string sourceName = "内置演示", statusMessage;
std::vector<std::string> lines;
int selected = 0, definitionPage = 0, candidatePage = 0, activeDictionary = -1;
bool sdMounted = false, usingSd = false, pinyin = false, dirty = true;
bool keyWasDown[56] = {};
bool searchPending = false;
uint32_t searchAt = 0;
enum class Page { Search, Definition, Help };
Page page = Page::Search;

M5GFX& screen() { return M5Cardputer.Display; }

void printAt(const std::string& text, int x, int y, uint16_t color = FG) {
    canvas.setTextColor(color);
    canvas.setCursor(x, y);
    canvas.print(text.c_str());
}

std::string fitEnd(std::string text, int width) {
    while (!text.empty() && canvas.textWidth(text.c_str()) > width) dict::popUtf8(text);
    return text;
}

std::string fitTail(std::string text, int width) {
    while (!text.empty() && canvas.textWidth(text.c_str()) > width)
        text.erase(0, dict::utf8Length(uint8_t(text[0])));
    return text;
}

void paintProgress(uint32_t done, uint32_t total) {
    screen().setFont(&kBodyFont);
    screen().setTextColor(FG, BG);
    screen().fillScreen(BG);
    screen().setCursor(8, 30);
    screen().print("正在建立词库索引");
    screen().setCursor(8, 55);
    screen().printf("%lu / %lu", (unsigned long)done, (unsigned long)total);
    screen().drawRect(8, 85, 224, 8, MUTED);
    screen().fillRect(9, 86, total ? uint64_t(done) * 222 / total : 0, 6, ACCENT);
    delay(1);
}

void updateCandidates() {
    candidatePage = 0;
    candidates.clear();
    if (composition.empty()) return;
    size_t lo = 0, hi = kPinyinCount;
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (composition.compare(kPinyin[mid].code) > 0) lo = mid + 1; else hi = mid;
    }
    if (lo < kPinyinCount && composition == kPinyin[lo].code) candidates = kPinyin[lo].candidates;
}

void refreshMatches() {
    selected = 0;
    matches.clear(); demoMatches.clear();
    if (usingSd) matches = dictionary.suggest(query, kRows);
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
    candidatePage = 0;
    page = Page::Search;
    refreshMatches();
}

bool loadDictionary(const std::string& ifoPath) {
    usingSd = false;
    indexFile.file.close(); bodyFile.file.close();
    File ifo = SD.open(ifoPath.c_str(), FILE_READ);
    if (!ifo || ifo.size() > 16384) { statusMessage = "Cannot read .ifo"; return false; }
    std::string source;
    source.reserve(ifo.size());
    while (ifo.available()) source.push_back(char(ifo.read()));
    ifo.close();
    dict::Metadata meta;
    std::string error;
    if (!dict::parseMetadata(source, meta, error)) { statusMessage = error; return false; }
    const auto prefix = ifoPath.substr(0, ifoPath.size() - 4);
    indexFile.file = SD.open((prefix + ".idx").c_str(), FILE_READ);
    bodyFile.file = SD.open((prefix + ".dict").c_str(), FILE_READ);
    if (!indexFile.file || !bodyFile.file) { statusMessage = "Need .idx and uncompressed .dict"; return false; }
    uint32_t lastDraw = 0;
    if (!dictionary.open(indexFile, bodyFile, meta, error, [&](uint32_t done, uint32_t total) {
        delay(1);
        if (done == total || millis() - lastDraw >= 250) {
            paintProgress(done, total);
            lastDraw = millis();
        }
    })) {
        statusMessage = error;
        Serial.printf("Dictionary error: %s\n", error.c_str());
        return false;
    }
    sourceName = meta.name;
    statusMessage.clear();
    usingSd = true;
    Serial.printf("Loaded %s: %lu headwords, %u sparse index bytes\n", sourceName.c_str(),
                  (unsigned long)dictionary.count(), unsigned(dictionary.indexRamBytes()));
    return true;
}

void scanSd() {
    usingSd = false;
    indexFile.file.close(); bodyFile.file.close();
    dictionaryPaths.clear(); activeDictionary = -1;
    if (sdMounted) SD.end();
    SPI.begin(40, 39, 14, 12);
    sdMounted = false;
    // Some cards need an extra initialization attempt after a USB warm reset.
    for (const uint32_t frequency : {10000000u, 4000000u, 1000000u}) {
        delay(100);
        if (SD.begin(12, SPI, frequency) && SD.cardType() != CARD_NONE) {
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
                    if (base.rfind("._", 0) != 0) dictionaryPaths.push_back("/dictionaries/" + base);
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
            if (loadDictionary(dictionaryPaths[i])) { activeDictionary = int(i); break; }
        }
        if (!usingSd) {
            sourceName = "内置演示";
            if (statusMessage.empty()) statusMessage = "SD 内未找到词库";
        }
    }
    clearQuery();
}

void nextDictionary() {
    if (dictionaryPaths.empty()) { scanSd(); return; }
    int next = activeDictionary + 1;
    if (next >= int(dictionaryPaths.size())) {
        usingSd = false; activeDictionary = -1; sourceName = "内置演示";
        indexFile.file.close(); bodyFile.file.close(); statusMessage.clear();
    } else {
        activeDictionary = next;
        if (!loadDictionary(dictionaryPaths[next])) {
            usingSd = false; sourceName = "内置演示";
        }
    }
    clearQuery();
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
        const auto glyph = definition.substr(pos, bytes);
        pos += bytes;
        if (!current.empty() && canvas.textWidth((current + glyph).c_str()) > 228) {
            lines.push_back(current); current.clear();
        }
        current += c == '\t' ? " " : glyph;
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
        definition = dictionary.definition(entry, truncated, ok);
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
    auto character = dict::utf8At(candidates, size_t(candidatePage * 5 + number));
    if (character.empty() || query.size() + character.size() > 120) return;
    query += character;
    composition.clear(); candidates.clear(); candidatePage = 0;
    scheduleSearch();
}

void move(int amount) {
    if (page == Page::Definition) {
        const int maxPage = int((lines.size() - 1) / kRows);
        definitionPage = std::max(0, std::min(maxPage, definitionPage + amount));
    } else if (page == Page::Search) {
        if (searchPending) refreshMatches();
        const int total = usingSd ? matches.size() : demoMatches.size();
        const int count = composition.empty() ? total : std::min(3, total);
        selected = std::max(0, std::min(std::max(0, count - 1), selected + amount));
    }
    dirty = true;
}

void handleKey(int x, int y, const Keyboard_Class::KeysState& state) {
    const auto key = M5Cardputer.Keyboard.getKeyValue({x, y});
    const char base = key.value_first;
    if (state.fn) {
        if (base == 'h') { page = page == Page::Help ? Page::Search : Page::Help; dirty = true; }
        else if (base == 'r') scanSd();
        else if (base == 'd') nextDictionary();
        else if (x == 13 && y == 0) clearQuery();
        else if (base == ';') move(-1);
        else if (base == '.') move(1);
        else if (base == ',') move(-1);
        else if (base == '/') move(1);
        return;
    }
    if (x == 0 && y == 1) {
        pinyin = !pinyin; composition.clear(); candidates.clear(); candidatePage = 0;
        page = Page::Search; dirty = true; return;
    }
    if (base == '`') {
        if (page != Page::Search) page = Page::Search;
        else if (!composition.empty()) { composition.clear(); candidates.clear(); }
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
        if (c >= '1' && c <= '5') { chooseCandidate(c - '1'); return; }
        if (c == ' ') { chooseCandidate(0); return; }
        if (c == '[' || c == ']') {
            const int last = std::max(0, int((dict::utf8Count(candidates) + 4) / 5) - 1);
            candidatePage = std::max(0, std::min(last, candidatePage + (c == ']' ? 1 : -1)));
            dirty = true; return;
        }
    }
    if (pinyin && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) {
        if (composition.size() < 6) composition += char(dict::fold(uint8_t(c)));
        updateCandidates(); dirty = true;
    } else if (c >= 32 && c <= 126 && query.size() < 120) {
        query += c; scheduleSearch();
    }
}

void render() {
    canvas.fillSprite(BG);
    canvas.setTextWrap(false);
    canvas.setFont(&fonts::efontCN_12);
    canvas.fillRect(0, 0, 240, 18, PANEL);
    printAt("CARDPUTER DICT", 5, 2, ACCENT);
    printAt(pinyin ? "中文" : "EN", 205, 2, ACCENT);
    if (page == Page::Help) {
        canvas.setFont(&kBodyFont);
        const char* help[] = {"Tab: 中英切换  Enter: 查词", "Fn+; / Fn+.: 选择或翻页", "拼音: 空格首选  1-5选字", "[ / ]: 候选翻页  v代表ü", "Fn+D: 切换词库  Fn+R: 重载", "Fn+退格: 清空  `: 返回"};
        for (size_t i = 0; i < 6; ++i) printAt(help[i], 4, 23 + int(i) * 16);
    } else if (page == Page::Definition) {
        canvas.setFont(&fonts::efontCN_16);
        printAt(fitEnd(headword, 228), 5, 21, ACCENT);
        canvas.drawFastHLine(4, 40, 232, SELECT);
        canvas.setFont(&kBodyFont);
        for (int row = 0; row < kRows; ++row) {
            const size_t i = size_t(definitionPage * kRows + row);
            if (i < lines.size()) printAt(lines[i], 5, 43 + row * 15);
        }
        canvas.setFont(&fonts::efontCN_12);
        const std::string footer = std::to_string(definitionPage + 1) + "/" + std::to_string((lines.size() + kRows - 1) / kRows) + "  Space:下一页  `:返回";
        printAt(footer, 5, 121, MUTED);
    } else {
        canvas.setFont(&fonts::efontCN_16);
        printAt(query.empty() ? "输入单词…" : fitTail(query, 214), 5, 21, query.empty() ? MUTED : FG);
        canvas.drawFastHLine(4, 40, 232, SELECT);
        canvas.setFont(&kBodyFont);
        const size_t count = usingSd ? matches.size() : demoMatches.size();
        const size_t visible = composition.empty() ? kRows : 3;
        for (size_t row = 0; row < std::min(count, visible); ++row) {
            const int y = 43 + row * 15;
            const auto word = usingSd ? matches[row].word : std::string(kDemo[demoMatches[row]].word);
            if (int(row) == selected) canvas.fillRoundRect(3, y - 1, 234, 15, 2, SELECT);
            canvas.setTextColor(FG);
            canvas.setCursor(7, y);
            canvas.print(fitEnd(word, 222).c_str());
        }
        if (count == 0) printAt("未找到；可换词库或缩短输入", 5, 45, MUTED);
        if (!composition.empty()) {
            canvas.fillRect(0, 88, 240, 33, PANEL);
            canvas.setFont(&fonts::efontCN_12);
            printAt("拼音 " + composition + "  " + std::to_string(candidatePage + 1), 4, 88, ACCENT);
            canvas.setFont(&fonts::efontCN_16);
            std::string row;
            for (int i = 0; i < 5; ++i) {
                const auto c = dict::utf8At(candidates, candidatePage * 5 + i);
                if (!c.empty()) row += std::to_string(i + 1) + c + " ";
            }
            printAt(row.empty() ? "无候选，请检查拼音" : row, 4, 102);
        }
        canvas.setFont(&fonts::efontCN_12);
        const std::string footer = usingSd ? "SD " + std::to_string(dictionary.count()) + "词  Fn+H帮助" : "演示 " + std::to_string(kDemoCount) + "词  Fn+H帮助";
        printAt(footer, 4, 121, MUTED);
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
    if (sscanf(request.c_str(), "put %63s %lu %lx", name, &length, &expectedHash) != 3 ||
        !length || length > 64 * 1024 * 1024 || !sdMounted) {
        Serial.println("ERR invalid request or SD unavailable"); return;
    }
    const std::string base = name;
    if (base.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-") != std::string::npos ||
        base[0] == '.' || base.find("..") != std::string::npos) {
        Serial.println("ERR invalid filename"); return;
    }
    const auto dot = base.find_last_of('.');
    if (dot == std::string::npos || (base.substr(dot) != ".ifo" && base.substr(dot) != ".idx" && base.substr(dot) != ".dict" &&
        base.substr(dot) != ".txt" && base.substr(dot) != ".json")) {
        Serial.println("ERR only dictionary data and license files allowed"); return;
    }
    const std::string target = "/dictionaries/" + base;
    const std::string partial = "/dictionaries/." + base + ".part";
    uint8_t buffer[4096];
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
    if (!SD.exists("/dictionaries") && !SD.mkdir("/dictionaries")) { Serial.println("ERR mkdir failed"); return; }
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
            screen().setTextColor(FG); screen().setCursor(6, 30);
            screen().print("USB 词库传输中"); screen().setCursor(6, 55); screen().print(name);
            screen().setCursor(6, 80); screen().printf("%lu / %lu KB", (unsigned long)(received / 1024), length / 1024);
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
    else Serial.println("DONE verified");
    dirty = true;
}

void serialCommand() {
    static std::string command;
    static bool overflow = false;
    while (Serial.available()) {
        const char c = char(Serial.read());
        if (c == '\r') continue;
        if (c == '\n') {
            if (overflow) Serial.println("ERR command too long");
            else if (command.rfind("put ", 0) == 0) receiveFile(command);
            else if (command == "status") {
                Serial.printf("CardputerDict 0.1.0 board=%d heap=%u sd=%d words=%lu source=%s\n", int(M5.getBoard()), ESP.getFreeHeap(), sdMounted,
                              (unsigned long)(usingSd ? dictionary.count() : kDemoCount), sourceName.c_str());
                Serial.printf("display=%dx%d rotation=%u\n", screen().width(), screen().height(), unsigned(screen().getRotation()));
                Serial.printf("note=%s\n", statusMessage.c_str());
            } else if (command == "reload") scanSd();
            else if (command == "screenshot") {
                if (dirty) render();
                Serial.print("PPM_BEGIN\nP6\n240 135\n255\n");
                uint8_t pixels[240 * 3];
                for (int y = 0; y < 135; ++y) {
                    canvas.readRectRGB(0, y, 240, 1, pixels);
                    Serial.write(pixels, sizeof(pixels));
                }
                Serial.print("\nPPM_END\n");
            } else if (command.rfind("ime ", 0) == 0 && command.size() <= 10) {
                composition = command.substr(4); pinyin = true;
                page = Page::Search; updateCandidates(); dirty = true;
                Serial.printf("CANDIDATES: %s\n", candidates.c_str());
            }
            else if (command.rfind("lookup ", 0) == 0) {
                query = command.substr(7); composition.clear(); candidates.clear();
                refreshMatches();
                const std::string first = usingSd ? (matches.empty() ? "" : matches[0].word) : (demoMatches.empty() ? "" : kDemo[demoMatches[0]].word);
                if (dict::compare(first, query) != 0 || first.empty()) Serial.println("NOT_FOUND");
                else { openSelected(); Serial.println(definition.c_str()); }
            } else if (!command.empty()) Serial.println("Commands: status | lookup WORD | reload | ime PINYIN | screenshot");
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
    cfg.internal_spk = false;
    cfg.internal_mic = false;
    M5Cardputer.begin(cfg, true);
    screen().setRotation(0);  // M5GFX includes Cardputer's panel rotation offset.
    screen().setBrightness(150);
    canvas.setColorDepth(16);
    spriteReady = canvas.createSprite(screen().width(), screen().height()) != nullptr;
    if (!spriteReady) {
        screen().fillScreen(BG); screen().setTextColor(FG);
        screen().println("Not enough RAM for screen buffer");
        return;
    }
    scanSd();
    query = "apple";
    refreshMatches();
    render();
    Serial.println("CardputerDict ready. Commands: status, lookup apple, reload");
}

void loop() {
    if (!spriteReady) { delay(100); return; }
    M5Cardputer.update();
    const auto& state = M5Cardputer.Keyboard.keysState();
    bool down[56] = {};
    for (const auto& point : M5Cardputer.Keyboard.keyList()) {
        if (point.x < 0 || point.x >= 14 || point.y < 0 || point.y >= 4) continue;
        const int i = point.y * 14 + point.x;
        down[i] = true;
        if (!keyWasDown[i]) handleKey(point.x, point.y, state);
    }
    memcpy(keyWasDown, down, sizeof(down));
    if (searchPending && int32_t(millis() - searchAt) >= 0) refreshMatches();
    if (M5Cardputer.BtnA.wasPressed()) { page = page == Page::Help ? Page::Search : Page::Help; dirty = true; }
    serialCommand();
    if (dirty) render();
    delay(5);
}
