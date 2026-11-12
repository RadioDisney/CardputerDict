#include "audio_core.h"
#include <cassert>
#include <iostream>

class Reader : public dict::Reader {
public:
    std::vector<uint8_t> bytes;
    uint32_t size() const override { return bytes.size(); }
    size_t readAt(uint32_t pos, uint8_t* output, size_t length) override {
        if (pos >= bytes.size()) return 0;
        length = std::min(length, bytes.size() - pos);
        memcpy(output, bytes.data() + pos, length); return length;
    }
};
void u32(std::vector<uint8_t>& bytes, uint32_t value) {
    const size_t offset = bytes.size(); bytes.resize(offset + 4); dict::putLittle32(bytes.data() + offset, value);
}
Reader wave(uint16_t channels, uint16_t bits) {
    Reader result;
    result.bytes = {'R','I','F','F',0,0,0,0,'W','A','V','E','J','U','N','K',1,0,0,0,123,0,'f','m','t',' ',16,0,0,0};
    const uint32_t align = channels * bits / 8;
    result.bytes.push_back(1); result.bytes.push_back(0);
    result.bytes.push_back(channels); result.bytes.push_back(0);
    u32(result.bytes, 16000); u32(result.bytes, 16000 * align);
    result.bytes.push_back(align); result.bytes.push_back(0);
    result.bytes.push_back(bits); result.bytes.push_back(0);
    for (char c : std::string("data")) result.bytes.push_back(c);
    u32(result.bytes, align * 100);
    result.bytes.insert(result.bytes.end(), align * 100, 0);
    dict::putLittle32(result.bytes.data() + 4, result.size() - 8);
    return result;
}

int main() {
    std::string normalized;
    assert(pronunciation::normalize("  TESLA\t ", normalized) && normalized == "tesla");
    assert(pronunciation::normalize("Apple   butter", normalized) && normalized == "apple butter");
    assert(pronunciation::normalize("Don't", normalized) && pronunciation::encode(normalized) == "don%27t");
    assert(pronunciation::normalize("U.S.", normalized) && pronunciation::encode(normalized) == "u%2es%2e");
    for (const auto& invalid : {"", "123", "...", "苹果", "../apple", "https://host/apple", "a?b=c", "a\nb", "a%2fb", "a\\b"})
        assert(!pronunciation::normalize(invalid, normalized));
    assert(!pronunciation::normalize(std::string(65,'a'), normalized));
    assert(pronunciation::normalize(std::string(64,'a'), normalized));
    for (const int channels : {1, 2}) for (const int bits : {8, 16}) {
        auto valid = wave(channels, bits);
        pronunciation::Wave parsed;
        assert(pronunciation::parseWave(valid, parsed));
        assert(parsed.channels == channels && parsed.bits == bits && parsed.rate == 16000);
        assert(parsed.bytes == uint32_t(channels * bits / 8 * 100) && parsed.offset == 54);
        for (size_t offset : {size_t(0), size_t(8), size_t(22), size_t(26), size_t(30), size_t(32), size_t(34), size_t(38), size_t(42), size_t(44)}) {
            auto invalid = valid; invalid.bytes[offset] = 0xff;
            assert(!pronunciation::parseWave(invalid, parsed));
        }
        for (size_t length : {size_t(0), size_t(12), size_t(43), valid.bytes.size() - 1}) {
            auto invalid = valid; invalid.bytes.resize(length);
            assert(!pronunciation::parseWave(invalid, parsed));
        }
    }
    std::cout << "Audio core tests passed: safe word names/URLs, mono/stereo PCM WAV, padding, corruption and bounds.\n";
}
