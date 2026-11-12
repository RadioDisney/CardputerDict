#include "pronunciation.h"
#include "audio_core.h"
#include "pronunciation_ca.h"
#include "keyboard_input.h"
#include <M5Cardputer.h>
#include <Preferences.h>
#include <SD.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <dr_mp3.h>
#include <memory>
#include <new>
#include <ctime>
#include <esp_heap_caps.h>

namespace pronunciation {
namespace {
enum class State { Idle, Connecting, Clock, Download, Playing, Drain };
State state = State::Idle;
std::string currentWord, note, ssid, password, currentPath;
bool dirty = false;
bool downloadCancelled = false;
uint32_t phaseStarted = 0, playbackStarted = 0;
uint8_t loudness = 96;
void (*prepareNetwork)() = nullptr;
File source;
std::unique_ptr<drmp3> decoder;
Wave wave;
uint32_t remaining = 0, rate = 0, channels = 0;
uint64_t framesPlayed = 0;
uint32_t peak = 0;
bool wav = false;
uint8_t slot = 0;
int16_t pcm[3][512 * 2] = {};

class FileReader : public dict::Reader {
public:
    uint32_t size() const override { return source ? source.size() : 0; }
    size_t readAt(uint32_t offset, uint8_t* bytes, size_t size) override {
        return source && source.seek(offset) ? source.read(bytes, size) : 0;
    }
};

void setNote(const std::string& message) { note = message; dirty = true; }
void networkOff() {
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
}
void closeSource() {
    if (decoder) { drmp3_uninit(decoder.get()); decoder.reset(); }
    source.close();
}
void fail(const std::string& message) {
    if (M5Cardputer.Speaker.isRunning()) { M5Cardputer.Speaker.stop(0); M5Cardputer.Speaker.end(); }
    closeSource();
    if (state == State::Connecting || state == State::Clock || state == State::Download) networkOff();
    state = State::Idle; keyboard::audioMode(false); setNote(message);
    Serial.printf("AUDIO error=%s\n", message.c_str());
}

size_t readMp3(void*, void* output, size_t bytes) {
    return source.read(static_cast<uint8_t*>(output), bytes);
}
drmp3_bool32 seekMp3(void*, int offset, drmp3_seek_origin origin) {
    const int64_t base = origin == DRMP3_SEEK_SET ? 0 : origin == DRMP3_SEEK_CUR ? source.position() : source.size();
    const int64_t position = base + offset;
    return position >= 0 && position <= source.size() && source.seek(uint32_t(position));
}
drmp3_bool32 tellMp3(void*, drmp3_int64* position) { *position = source.position(); return true; }

bool openAudio(const std::string& path) {
    closeSource();
    source = SD.open(path.c_str(), FILE_READ);
    if (!source || source.size() < 32 || source.size() > kMaxAudioBytes) {
        Serial.printf("AUDIO invalid_file path=%s size=%lu\n", path.c_str(), (unsigned long)(source ? source.size() : 0));
        closeSource(); return false;
    }
    uint8_t header[12];
    if (source.read(header, sizeof(header)) != sizeof(header) || !source.seek(0)) { Serial.println("AUDIO header_read_failed"); closeSource(); return false; }
    wav = memcmp(header, "RIFF", 4) == 0;
    if (wav) {
        FileReader reader;
        if (!parseWave(reader, wave) || !source.seek(wave.offset)) { closeSource(); return false; }
        remaining = wave.bytes; rate = wave.rate; channels = wave.channels;
    } else {
        decoder.reset(new (std::nothrow) drmp3{});
        if (!decoder || !drmp3_init(decoder.get(), readMp3, seekMp3, tellMp3, nullptr, nullptr, nullptr)) {
            Serial.printf("AUDIO mp3_init_failed heap=%u largest=%u decoder_bytes=%u\n", ESP.getFreeHeap(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT), unsigned(sizeof(drmp3)));
            decoder.reset(); source.close(); return false;
        }
        rate = decoder->sampleRate; channels = decoder->channels;
        if (rate < 8000 || rate > 48000 || (channels != 1 && channels != 2)) { closeSource(); return false; }
    }
    currentPath = path;
    return true;
}

bool startPlayback(const std::string& path) {
    keyboard::audioMode(true);
    if (!openAudio(path)) { keyboard::audioMode(false); return false; }
    if (!M5Cardputer.Speaker.begin()) { closeSource(); keyboard::audioMode(false); return false; }
    M5Cardputer.Speaker.setVolume(loudness);
    state = State::Playing; playbackStarted = millis(); framesPlayed = peak = slot = 0;
    setNote("正在朗读 " + currentWord);
    Serial.printf("AUDIO start word=%s format=%s rate=%lu channels=%lu path=%s\n", currentWord.c_str(), wav ? "wav" : "mp3",
                  (unsigned long)rate, (unsigned long)channels, path.c_str());
    return true;
}

class DownloadSink : public Stream {
public:
    explicit DownloadSink(File& file) : output(file) {}
    File& output;
    uint32_t count = 0, crc = 0xffffffffu;
    bool failed = false;
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* data, size_t length) override {
        if (failed || millis() - started > 25000 || length > kMaxAudioBytes - count) { failed = true; return 0; }
        const size_t written = output.write(data, length);
        crc = dict::crc32(data, written, crc); count += written;
        if (written != length) failed = true;
        // Cancel during streaming without exposing credentials or touching UI state.
        M5.update();
        if (keyboard::stopRequested()) {
            failed = downloadCancelled = true;
        }
        delay(1);
        return failed ? 0 : written;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override { output.flush(); }
private:
    uint32_t started = millis();
};

bool fetchAudio(const std::string& temporary) {
    downloadCancelled = false;
    WiFiClientSecure connection;
    connection.setCACert(kPronunciationRootCA);
    connection.setHandshakeTimeout(8);
    HTTPClient request;
    request.setConnectTimeout(5000); request.setTimeout(5000);
    request.useHTTP10(true);
    const std::string url = "https://dict.youdao.com/dictvoice?audio=" + encode(currentWord) + "&type=2";
    if (!request.begin(connection, url.c_str())) return false;
    request.addHeader("Accept", "audio/mpeg");
    const int code = request.GET();
    const int expected = request.getSize();
    if (code != 200 || expected > int(kMaxAudioBytes)) {
        Serial.printf("AUDIO http_status=%d bytes=%d\n", code, expected); request.end(); return false;
    }
    File output = SD.open(temporary.c_str(), FILE_WRITE);
    if (!output) { request.end(); return false; }
    DownloadSink sink(output);
    const int received = request.writeToStream(&sink);
    output.flush(); output.close(); request.end(); connection.stop();
    if (received < 32 || sink.failed || sink.count != uint32_t(received) || (expected >= 0 && received != expected)) return false;
    File verify = SD.open(temporary.c_str(), FILE_READ);
    if (!verify || verify.size() != sink.count) return false;
    uint8_t buffer[1024];
    uint32_t checked = 0, crc = 0xffffffffu;
    while (verify.available()) {
        const size_t n = verify.read(buffer, sizeof(buffer));
        if (!n) break;
        crc = dict::crc32(buffer, n, crc); checked += n; delay(1);
    }
    verify.close();
    return checked == sink.count && crc == sink.crc;
}
} // namespace

void setPrepareNetwork(void (*callback)()) { prepareNetwork = callback; }

void begin() {
    Preferences preferences;
    if (preferences.begin("carddict-audio", true)) {
        ssid = preferences.getString("ssid", "").c_str();
        password = preferences.getString("password", "").c_str();
        loudness = preferences.getUChar("volume", 96);
        preferences.end();
    }
}
bool configured() { return !ssid.empty(); }
std::string savedSsid() { return ssid; }
bool saveWifi(const std::string& network, const std::string& secret) {
    if (network.empty() || network.size() > 32 || secret.size() > 63 || (!secret.empty() && secret.size() < 8)) return false;
    Preferences preferences;
    if (!preferences.begin("carddict-audio", false)) return false;
    const bool ok = preferences.putString("ssid", network.c_str()) == network.size() &&
                    preferences.putString("password", secret.c_str()) == secret.size();
    preferences.end();
    if (ok) { ssid = network; password = secret; setNote("WiFi 已保存"); }
    return ok;
}
bool request(const std::string& value, bool sdReady, bool allowOnline) {
    stop();
    framesPlayed = peak = 0; currentPath.clear(); rate = channels = 0;
    if (!normalize(value, currentWord)) { setNote("请选择英文词条朗读"); return false; }
    if (!sdReady) { setNote("朗读需要 SD 卡"); return false; }
    const auto key = encode(currentWord);
    for (const auto& path : {"/audio/en/" + key + ".mp3", "/audio/en/" + key + ".wav", "/audio/cache/en/" + key + ".mp3"}) {
        if (SD.exists(path.c_str())) {
            Serial.printf("AUDIO local=%s\n", path.c_str());
            if (prepareNetwork && (ESP.getFreeHeap() < sizeof(drmp3) + 16384 + 30000 ||
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < sizeof(drmp3) + 2048)) prepareNetwork();
            if (startPlayback(path)) return true;
        }
    }
    if (!allowOnline) { setNote("没有本地音频"); return false; }
    if (!configured()) { setNote("无本地音频，Fn+W 配置WiFi"); return false; }
    keyboard::audioMode(true);
    if (prepareNetwork) prepareNetwork();
    WiFi.persistent(false); WiFi.setAutoReconnect(false); WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    state = State::Connecting; phaseStarted = millis(); setNote("正在连接 WiFi");
    return true;
}
void tick() {
    if (state == State::Connecting) {
        if (WiFi.status() == WL_CONNECTED) {
            state = State::Clock; phaseStarted = millis();
            if (time(nullptr) < 1700000000) configTime(0, 0, "ntp.aliyun.com", "pool.ntp.org");
            setNote("正在校时，准备获取发音");
        } else if (millis() - phaseStarted > 15000) fail("WiFi 连接失败，Fn+W 检查设置");
    } else if (state == State::Clock) {
        if (time(nullptr) >= 1700000000) { state = State::Download; setNote("正在获取并缓存发音"); }
        else if (millis() - phaseStarted > 10000) fail("网络校时失败，请稍后重试");
    } else if (state == State::Download) {
        if (!SD.exists("/audio")) SD.mkdir("/audio");
        if (!SD.exists("/audio/cache")) SD.mkdir("/audio/cache");
        if (!SD.exists("/audio/cache/en")) SD.mkdir("/audio/cache/en");
        const auto target = "/audio/cache/en/" + encode(currentWord) + ".mp3";
        const auto temporary = target + ".part";
        if (SD.totalBytes() - SD.usedBytes() < kMaxAudioBytes + 65536) { fail("SD 卡剩余空间不足"); return; }
        if (SD.exists(temporary.c_str())) SD.remove(temporary.c_str());
        const bool downloaded = fetchAudio(temporary);
        networkOff();
        if (downloadCancelled) { SD.remove(temporary.c_str()); fail("已停止朗读"); return; }
        // Decode only after the HTTPS buffers have been released.
        if (!downloaded || !openAudio(temporary)) {
            closeSource(); SD.remove(temporary.c_str()); fail("发音获取失败，请检查网络"); return;
        }
        closeSource();
        if (SD.exists(target.c_str())) SD.remove(target.c_str());
        if (!SD.rename(temporary.c_str(), target.c_str()) || !startPlayback(target)) {
            SD.remove(temporary.c_str()); fail("发音缓存无法读取"); return;
        }
        Serial.printf("AUDIO cached=%s\n", target.c_str());
    } else if (state == State::Playing) {
        if (millis() - playbackStarted > kMaxDurationSeconds * 1000 + 3000) { fail("音频超过播放时限"); return; }
        if (M5Cardputer.Speaker.isPlaying(0) >= 2) return;
        auto* output = pcm[slot];
        size_t frames = 0;
        if (wav) {
            const size_t bytesPerFrame = channels * wave.bits / 8;
            frames = std::min(size_t(512), size_t(remaining / bytesPerFrame));
            const size_t samples = frames * channels;
            if (wave.bits == 16) {
                const size_t bytes = samples * 2;
                if (source.read(reinterpret_cast<uint8_t*>(output), bytes) != bytes) { fail("WAV 读取失败"); return; }
            } else {
                // Read 8-bit PCM into the second half, then expand in place.
                auto* input = reinterpret_cast<uint8_t*>(output) + samples;
                if (source.read(input, samples) != samples) { fail("WAV 读取失败"); return; }
                for (size_t i = 0; i < samples; ++i) output[i] = (int(input[i]) - 128) * 256;
            }
            remaining -= frames * bytesPerFrame;
        } else frames = drmp3_read_pcm_frames_s16(decoder.get(), 512, output);
        if (!frames) { state = State::Drain; return; }
        for (size_t i = 0; i < frames * channels; ++i) peak = std::max(peak, uint32_t(std::abs(int(output[i]))));
        if (!M5Cardputer.Speaker.playRaw(output, frames * channels, rate, channels == 2, 1, 0, false)) { fail("扬声器播放失败"); return; }
        framesPlayed += frames; slot = (slot + 1) % 3;
    } else if (state == State::Drain && !M5Cardputer.Speaker.isPlaying(0)) {
        M5Cardputer.Speaker.end();
        closeSource(); state = State::Idle; keyboard::audioMode(false); setNote("朗读完成 " + currentWord);
        Serial.printf("AUDIO done word=%s frames=%llu peak=%lu\n", currentWord.c_str(), (unsigned long long)framesPlayed, (unsigned long)peak);
    }
}
void stop() {
    if (state == State::Connecting || state == State::Clock || state == State::Download) networkOff();
    if (M5Cardputer.Speaker.isRunning()) { M5Cardputer.Speaker.stop(0); M5Cardputer.Speaker.end(); }
    closeSource();
    if (state != State::Idle) setNote("已停止朗读");
    state = State::Idle;
    keyboard::audioMode(false);
}
bool busy() { return state != State::Idle; }
bool downloading() { return state == State::Connecting || state == State::Clock || state == State::Download; }
bool changed() { const bool value = dirty; dirty = false; return value; }
const std::string& word() { return currentWord; }
const std::string& message() { return note; }
const char* stateName() {
    switch (state) {
    case State::Connecting: return "connecting";
    case State::Clock: return "clock";
    case State::Download: return "download";
    case State::Playing: return "playing";
    case State::Drain: return "drain";
    default: return "idle";
    }
}
void volume(int delta) {
    loudness = uint8_t(std::max(0, std::min(255, int(loudness) + delta)));
    M5Cardputer.Speaker.setVolume(loudness);
    Preferences preferences;
    if (preferences.begin("carddict-audio", false)) { preferences.putUChar("volume", loudness); preferences.end(); }
    setNote("音量 " + std::to_string(int(loudness) * 100 / 255) + "%");
}
uint8_t volume() { return loudness; }
void diagnostics() {
    Serial.printf("AUDIO state=%s word=%s wifi_configured=%d wifi_connected=%d volume=%u frames=%llu peak=%lu path=%s heap=%u\n",
        stateName(), currentWord.c_str(), configured(), WiFi.status() == WL_CONNECTED, loudness,
        (unsigned long long)framesPlayed, (unsigned long)peak, currentPath.c_str(), ESP.getFreeHeap());
}
} // namespace pronunciation
