#include "keyboard_scan.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    keyboard::ScanFilter filter;
    const uint64_t letter = keyboard::keyBit(10, 2);
    assert(filter.update(0, letter) == 0);
    assert(filter.update(1, 0) == 0);
    assert(filter.update(2, letter) == 0);
    assert(filter.update(3, letter) == 0);
    assert(filter.update(4, letter) == letter);
    assert(filter.update(5, 0) == letter);
    assert(filter.update(6, letter) == letter);
    assert(filter.update(7, 0) == letter);
    assert(filter.update(8, 0) == letter);
    assert(filter.update(9, 0) == 0);
    assert(filter.update(10, letter) == 0);
    assert(filter.update(12, letter) == letter);

    // Capture consecutive l's and t's at 4ms release gaps even if the UI doesn't
    // consume any events until the entire word has already been typed.
    keyboard::ScanFilter fast;
    const std::string text = "letterll";
    auto bitFor = [](char c) { return c == 'l' ? keyboard::keyBit(10, 2) :
        c == 'e' ? keyboard::keyBit(3, 1) : c == 't' ? keyboard::keyBit(5, 1) : keyboard::keyBit(4, 1); };
    std::vector<keyboard::Event> delayedUiQueue;
    uint64_t previous = 0;
    for (uint32_t at = 0; at < 120; ++at) {
        const uint32_t tap = at / 8;
        const uint64_t raw = tap < text.size() && at % 8 < 4 ? bitFor(text[tap]) : 0;
        const uint64_t held = fast.update(at, raw);
        keyboard::emitChanges(previous, held, at, false, [&](auto event) { delayedUiQueue.push_back(event); });
        previous = held;
    }
    assert(delayedUiQueue.size() == text.size() * 2);
    std::string received;
    for (const auto& event : delayedUiQueue) {
        if (!event.pressed) continue;
        received += event.key == 38 ? 'l' : event.key == 17 ? 'e' : event.key == 19 ? 't' : 'r';
    }
    assert(received == text);
    assert(delayedUiQueue[2].at - delayedUiQueue[1].at == 4);

    const uint64_t chord = keyboard::keyBit(0, 2) | keyboard::keyBit(1, 2) | keyboard::keyBit(10, 1);
    assert(keyboard::modifiers(chord) == (keyboard::kFn | keyboard::kShift));
    std::vector<keyboard::Event> combined;
    keyboard::emitChanges(keyboard::keyBit(3, 1), chord, 42, true, [&](auto event) { combined.push_back(event); });
    assert(combined.size() == 4 && !combined.front().pressed);
    for (const auto& event : combined) assert(event.held == chord && event.audio && event.at == 42);
    keyboard::ScanFilter wrapped;
    assert(wrapped.update(UINT32_MAX - 1, letter) == 0);
    assert(wrapped.update(1, letter) == letter);
    std::cout << "Keyboard scan passed: 2ms debounce, bounce rejection, 4ms repeated-key gaps behind a blocked UI, modifiers and clock rollover.\n";
}
