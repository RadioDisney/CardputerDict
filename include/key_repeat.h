#pragma once
#include <cstdint>

// Emits an initial press followed by timed repeats. Suppressed combinations
// stay suppressed until physical release; slow frames never create a burst.
class KeyRepeat {
public:
    constexpr KeyRepeat(uint32_t delayMs = 450, uint32_t intervalMs = 125)
        : delay_(delayMs), interval_(intervalMs) {}
    bool update(uint32_t now, bool pressed, bool enabled = true) {
        if (!pressed) { held_ = armed_ = false; return false; }
        if (!enabled) { held_ = true; armed_ = false; return false; }
        if (!held_) {
            held_ = armed_ = true;
            next_ = now + delay_;
            return true;
        }
        if (armed_ && int32_t(now - next_) >= 0) {
            next_ = now + interval_;
            return true;
        }
        return false;
    }
private:
    uint32_t delay_, interval_, next_ = 0;
    bool held_ = false, armed_ = false;
};
