#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#define DR_MP3_NO_SIMD
#define DRMP3_DATA_CHUNK_SIZE 16384
#include "dr_mp3.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <vector>

struct Input { const std::vector<uint8_t>& bytes; size_t position = 0; };
size_t readInput(void* context, void* output, size_t count) {
    auto& input = *static_cast<Input*>(context);
    count = std::min(count, input.bytes.size() - input.position);
    memcpy(output, input.bytes.data() + input.position, count); input.position += count; return count;
}
drmp3_bool32 seekInput(void* context, int offset, drmp3_seek_origin origin) {
    auto& input = *static_cast<Input*>(context);
    const int64_t base = origin == DRMP3_SEEK_SET ? 0 : origin == DRMP3_SEEK_CUR ? input.position : input.bytes.size();
    const int64_t next = base + offset;
    if (next < 0 || uint64_t(next) > input.bytes.size()) return false;
    input.position = size_t(next); return true;
}
drmp3_bool32 tellInput(void* context, drmp3_int64* cursor) { *cursor = static_cast<Input*>(context)->position; return true; }

int main(int argc, char** argv) {
    assert(argc == 2 || argc == 3);
    std::ifstream file(argv[1], std::ios::binary);
    assert(file);
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    auto decoder = std::make_unique<drmp3>();
    assert(drmp3_init_memory(decoder.get(), bytes.data(), bytes.size(), nullptr));
    assert(decoder->channels >= 1 && decoder->channels <= 2 && decoder->sampleRate >= 8000 && decoder->sampleRate <= 48000);
    int16_t pcm[1024]; uint64_t total = 0; int peak = 0;
    std::vector<int16_t> recording;
    while (const auto frames = drmp3_read_pcm_frames_s16(decoder.get(), 512, pcm)) {
        for (size_t i = 0; i < frames * decoder->channels; ++i) peak = std::max(peak, std::abs(int(pcm[i])));
        total += frames;
        if (argc == 3) recording.insert(recording.end(), pcm, pcm + frames * decoder->channels);
        assert(total <= decoder->sampleRate * 30);
    }
    assert(total > decoder->sampleRate / 10 && peak > 100);
    if (argc == 3) {
        std::ofstream wav(argv[2], std::ios::binary);
        auto le = [&](uint32_t value, int bytes) { for (int i = 0; i < bytes; ++i) wav.put(char(value >> (i * 8))); };
        wav.write("RIFF",4); le(36 + recording.size() * 2,4); wav.write("WAVEfmt ",8); le(16,4); le(1,2);
        le(decoder->channels,2); le(decoder->sampleRate,4); le(decoder->sampleRate * decoder->channels * 2,4);
        le(decoder->channels*2,2); le(16,2); wav.write("data",4); le(recording.size()*2,4);
        for (const auto sample : recording) le(uint16_t(sample),2);
        assert(wav.good());
    }
    std::cout << "Real MP3 decoded: " << total << " frames, " << decoder->sampleRate << " Hz, peak=" << peak << ", decoder=" << sizeof(drmp3) << " bytes\n";
    drmp3_uninit(decoder.get());
    Input input{bytes};
    assert(drmp3_init(decoder.get(), readInput, seekInput, tellInput, nullptr, &input, nullptr));
    uint64_t streamed = 0; int streamPeak = 0;
    while (const auto frames = drmp3_read_pcm_frames_s16(decoder.get(), 512, pcm)) {
        for (size_t i = 0; i < frames * decoder->channels; ++i) streamPeak = std::max(streamPeak, std::abs(int(pcm[i])));
        streamed += frames;
    }
    assert(streamed == total && streamPeak == peak);
    assert(decoder->dataCapacity == 16384);
    drmp3_uninit(decoder.get());
    std::cout << "16 KB streamed MP3 matches memory decoding exactly.\n";
}
