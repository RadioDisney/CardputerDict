#pragma once
#include <cstddef>
#include "keyboard_scan.h"

namespace keyboard {
bool begin();
bool next(Event& event);
uint64_t heldKeys();
void audioMode(bool enabled);
bool stopRequested();
void diagnostics();
// Diagnostic raw tap traces pass through the same scanner/filter/event queue.
bool injectTaps(const uint8_t* keys, size_t count, uint32_t heldMs = 8, uint32_t gapMs = 4);
} // namespace keyboard
