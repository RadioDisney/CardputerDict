#pragma once
#include <cstdint>
#include <initializer_list>

namespace keyboard {
constexpr int kKeyCount = 56;
constexpr uint32_t kDebounceMs = 2;
constexpr uint64_t keyBit(int x, int y) { return uint64_t(1) << (y * 14 + x); }
constexpr uint8_t kFn = 1, kShift = 2, kCtrl = 4, kOpt = 8, kAlt = 16;

inline uint8_t modifiers(uint64_t held) {
    return ((held & keyBit(0, 2)) ? kFn : 0) | ((held & keyBit(1, 2)) ? kShift : 0) |
           ((held & keyBit(0, 3)) ? kCtrl : 0) | ((held & keyBit(1, 3)) ? kOpt : 0) |
           ((held & keyBit(2, 3)) ? kAlt : 0);
}

struct Event {
    uint64_t held = 0;
    uint32_t at = 0;
    uint8_t key = 0;
    bool pressed = false;
    // Captured while audio blocks typing: never replay it as normal text later.
    bool audio = false;
};

class ScanFilter {
public:
    uint64_t update(uint32_t now, uint64_t raw) {
        for (int key = 0; key < kKeyCount; ++key) {
            const uint64_t bit = uint64_t(1) << key;
            if ((raw ^ candidate_) & bit) {
                candidate_ ^= bit;
                changedAt_[key] = now;
            }
            if (((candidate_ ^ stable_) & bit) && uint32_t(now - changedAt_[key]) >= kDebounceMs) stable_ ^= bit;
        }
        return stable_;
    }
private:
    uint64_t candidate_ = 0, stable_ = 0;
    uint32_t changedAt_[kKeyCount] = {};
};

template <typename Emit>
void emitChanges(uint64_t previous, uint64_t held, uint32_t at, bool audio, Emit emit) {
    const uint64_t changed = previous ^ held;
    // Release before press: a rapid repeat queued behind a redraw is a new tap.
    for (const bool pressed : {false, true}) {
        for (int key = 0; key < kKeyCount; ++key) {
            const uint64_t bit = uint64_t(1) << key;
            if ((changed & bit) && bool(held & bit) == pressed) emit(Event{held, at, uint8_t(key), pressed, audio});
        }
    }
}
} // namespace keyboard
