#include "key_repeat.h"
#include <cassert>
#include <iostream>
#include <vector>

int main() {
    KeyRepeat key;
    std::vector<uint32_t> events;
    for (uint32_t now = 0; now <= 1000; now += 5)
        if (key.update(now, true)) events.push_back(now);
    assert((events == std::vector<uint32_t>{0, 450, 575, 700, 825, 950}));
    assert(!key.update(1005, false));
    assert(!key.update(5000, false));
    assert(key.update(5001, true));
    assert(!key.update(5050, false));
    assert(key.update(5060, true));
    assert(!key.update(5500, true));
    assert(key.update(5510, true));
    // Suppressed Fn/Ctrl/Alt combinations never restart repeats until release.
    assert(!key.update(5600, true, false));
    assert(!key.update(9000, true));
    assert(!key.update(9001, false));
    assert(key.update(9002, true));
    // A blocked frame gives one repeat, never accumulated catch-up deletions.
    assert(key.update(20000, true));
    assert(!key.update(20000, true));
    assert(!key.update(20124, true));
    assert(key.update(20125, true));
    // Timer arithmetic works across millis() rollover.
    KeyRepeat wrapped;
    const uint32_t start = UINT32_MAX - 200;
    assert(wrapped.update(start, true));
    assert(!wrapped.update(start + 449, true));
    assert(wrapped.update(start + 450, true));
    assert(!wrapped.update(start + 574, true));
    assert(wrapped.update(start + 575, true));
    std::cout << "Backspace repeat passed: short press, 450ms delay, 125ms repeats, release, modifiers, slow frames and timer rollover.\n";
}
