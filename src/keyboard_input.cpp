#include "keyboard_input.h"
#include <M5Cardputer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <algorithm>
#include <cstring>

namespace keyboard {
namespace {
QueueHandle_t events = nullptr;
TaskHandle_t scanner = nullptr;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
uint64_t stableHeld = 0;
bool audio = false;
uint32_t scans = 0, overflow = 0, maxIntervalUs = 0;
uint8_t testKeys[32] = {};
size_t testCount = 0;
uint32_t testStart = 0, testHoldMs = 8, testGapMs = 4;

void scan(void*) {
    ScanFilter filter;
    uint64_t previous = 0;
    uint32_t last = micros();
    for (;;) {
        // This task is the sole owner of the mutable keyboard reader. The UI
        // uses queued events and the immutable key map, never keysState().
        M5Cardputer.Keyboard.updateKeyList();
        uint64_t raw = 0;
        for (const auto& point : M5Cardputer.Keyboard.keyList()) {
            if (point.x >= 0 && point.x < 14 && point.y >= 0 && point.y < 4) raw |= keyBit(point.x, point.y);
        }
        const uint32_t now = millis();
        portENTER_CRITICAL(&lock);
        const int32_t testAge = int32_t(now - testStart);
        if (testCount && testAge >= 0) {
            const uint32_t period = testHoldMs + testGapMs;
            const size_t index = uint32_t(testAge) / period;
            if (index >= testCount) testCount = 0;
            else if (uint32_t(testAge) % period < testHoldMs) raw |= uint64_t(1) << testKeys[index];
        }
        portEXIT_CRITICAL(&lock);
        const uint64_t held = filter.update(now, raw);
        const uint32_t timestamp = micros();
        portENTER_CRITICAL(&lock);
        stableHeld = held;
        const bool audioContext = audio;
        ++scans;
        maxIntervalUs = std::max(maxIntervalUs, timestamp - last);
        portEXIT_CRITICAL(&lock);
        last = timestamp;
        emitChanges(previous, held, now, audioContext, [](const Event& event) {
            if (xQueueSend(events, &event, 0) != pdTRUE) {
                portENTER_CRITICAL(&lock); ++overflow; portEXIT_CRITICAL(&lock);
            }
        });
        previous = held;
        vTaskDelay(1);
    }
}
} // namespace

bool begin() {
    if (scanner) return true;
    events = xQueueCreate(96, sizeof(Event));
    if (!events) return false;
    if (xTaskCreatePinnedToCore(scan, "dict-keyboard", 3072, nullptr, 2, &scanner, 0) != pdPASS) {
        vQueueDelete(events); events = nullptr; scanner = nullptr; return false;
    }
    return true;
}
bool next(Event& event) { return events && xQueueReceive(events, &event, 0) == pdTRUE; }
uint64_t heldKeys() {
    portENTER_CRITICAL(&lock); const uint64_t value = stableHeld; portEXIT_CRITICAL(&lock); return value;
}
void audioMode(bool enabled) {
    portENTER_CRITICAL(&lock); audio = enabled; portEXIT_CRITICAL(&lock);
}
bool stopRequested() {
    const auto held = heldKeys();
    return (held & keyBit(0, 0)) || ((held & keyBit(0, 2)) && (held & keyBit(10, 1)));
}
bool injectTaps(const uint8_t* keys, size_t count, uint32_t heldMs, uint32_t gapMs) {
    if (!scanner || !count || count > sizeof(testKeys) || heldMs < 4 || heldMs > 2000 || gapMs < 4 || gapMs > 1000) return false;
    for (size_t i = 0; i < count; ++i) if (keys[i] >= kKeyCount) return false;
    portENTER_CRITICAL(&lock);
    if (testCount) { portEXIT_CRITICAL(&lock); return false; }
    memcpy(testKeys, keys, count); testCount = count; testHoldMs = heldMs; testGapMs = gapMs; testStart = millis() + 5;
    portEXIT_CRITICAL(&lock);
    return true;
}
void diagnostics() {
    portENTER_CRITICAL(&lock);
    const auto count = scans, dropped = overflow, maximum = maxIntervalUs;
    portEXIT_CRITICAL(&lock);
    Serial.printf("keyboard scanner=task scan_tick_ms=%u debounce_ms=%lu queued=%u dropped=%lu scans=%lu max_interval_us=%lu\n",
        unsigned(portTICK_PERIOD_MS), (unsigned long)kDebounceMs, events ? unsigned(uxQueueMessagesWaiting(events)) : 0,
        (unsigned long)dropped, (unsigned long)count, (unsigned long)maximum);
}
} // namespace keyboard
