#include "keyboard_input.h"
#include "device_hardware.h"
#include <M5Cardputer.h>
#include <utility/Adafruit_TCA8418/Adafruit_TCA8418.h>
#include <utility/Adafruit_TCA8418/Adafruit_TCA8418_registers.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <algorithm>
#include <cstring>
#include <memory>

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
std::unique_ptr<Adafruit_TCA8418> controller;
uint64_t advHeld = 0;
uint32_t fifoEvents = 0, fifoOverflows = 0, i2cErrors = 0;

void publish(uint64_t& previous, uint64_t held, uint32_t now) {
    portENTER_CRITICAL(&lock);
    stableHeld = held;
    const bool audioContext = audio;
    portEXIT_CRITICAL(&lock);
    emitChanges(previous, held, now, audioContext, [](const Event& event) {
        if (xQueueSend(events, &event, 0) != pdTRUE) {
            portENTER_CRITICAL(&lock); ++overflow; portEXIT_CRITICAL(&lock);
        }
    });
    previous = held;
}

void scan(void*) {
    ScanFilter filter;
    uint64_t previous = 0;
    uint64_t injectedHeld = 0;
    uint32_t last = micros();
    for (;;) {
        uint64_t raw = 0;
        uint64_t injectedRaw = 0;
        const uint32_t now = millis();
        if (controller) {
            // Adv already debounces in hardware. Consume EVERY FIFO edge before
            // the next poll; filtering only a final bitmap could lose fast taps.
            uint8_t status = 0, count = 0;
            if (!M5.In_I2C.readRegister(0x34, TCA8418_REG_INT_STAT, &status, 1, 400000) ||
                !M5.In_I2C.readRegister(0x34, TCA8418_REG_KEY_LCK_EC, &count, 1, 400000)) {
                portENTER_CRITICAL(&lock); ++i2cErrors; portEXIT_CRITICAL(&lock);
            } else {
                if (status & 0x08) {
                    portENTER_CRITICAL(&lock); ++fifoOverflows; portEXIT_CRITICAL(&lock);
                    advHeld = 0;
                    publish(previous, advHeld | injectedHeld, now);
                }
                for (uint8_t i = 0; i < std::min(uint8_t(10), uint8_t(count & 0x0f)); ++i) {
                    uint8_t event = 0;
                    if (!M5.In_I2C.readRegister(0x34, TCA8418_REG_KEY_EVENT_A, &event, 1, 400000)) {
                        portENTER_CRITICAL(&lock); ++i2cErrors; portEXIT_CRITICAL(&lock); break;
                    }
                    device::TcaEvent key;
                    if (!device::decodeTca(event, key)) continue;
                    portENTER_CRITICAL(&lock); ++fifoEvents; portEXIT_CRITICAL(&lock);
                    const uint64_t bit = uint64_t(1) << key.key;
                    if (key.pressed) advHeld |= bit; else advHeld &= ~bit;
                    // Hardware FIFO transitions bypass the software debounce. This
                    // preserves quick press/release edges from the Adv controller.
                    publish(previous, advHeld | injectedHeld, now);
                }
                if (status & 0x09) M5.In_I2C.writeRegister8(0x34, TCA8418_REG_INT_STAT, status & 0x09, 400000);
            }
        } else {
            // The normal Cardputer's GPIO matrix remains owned by this task.
            M5Cardputer.Keyboard.updateKeyList();
            for (const auto& point : M5Cardputer.Keyboard.keyList()) {
                if (point.x >= 0 && point.x < 14 && point.y >= 0 && point.y < 4) raw |= keyBit(point.x, point.y);
            }
        }
        portENTER_CRITICAL(&lock);
        const int32_t testAge = int32_t(now - testStart);
        if (testCount && testAge >= 0) {
            const uint32_t period = testHoldMs + testGapMs;
            const size_t index = uint32_t(testAge) / period;
            if (index >= testCount) testCount = 0;
            else if (uint32_t(testAge) % period < testHoldMs) injectedRaw |= uint64_t(1) << testKeys[index];
        }
        portEXIT_CRITICAL(&lock);
        const uint64_t held = filter.update(now, controller ? injectedRaw : (raw | injectedRaw));
        if (controller) injectedHeld = held;
        const uint32_t timestamp = micros();
        portENTER_CRITICAL(&lock);
        ++scans;
        maxIntervalUs = std::max(maxIntervalUs, timestamp - last);
        portEXIT_CRITICAL(&lock);
        last = timestamp;
        // Adv hardware transitions were queued above; merge in only the
        // software-filtered diagnostic injection for the final held snapshot.
        publish(previous, controller ? (advHeld | held) : held, now);
        vTaskDelay(1);
    }
}
} // namespace

bool begin() {
    if (scanner) return true;
    if (device::profile().model == device::Model::CardputerAdv) {
        if (!device::advKeyboardPresent()) return false;
        controller.reset(new (std::nothrow) Adafruit_TCA8418());
        if (!controller || !controller->begin() || !controller->matrix(7, 8)) { controller.reset(); return false; }
        controller->flush(); controller->enableDebounce(); controller->enableInterrupts();
    }
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
    const auto fifo = fifoEvents, overruns = fifoOverflows, errors = i2cErrors;
    portEXIT_CRITICAL(&lock);
    Serial.printf("keyboard scanner=task scan_tick_ms=%u debounce_ms=%lu queued=%u dropped=%lu scans=%lu max_interval_us=%lu\n",
        unsigned(portTICK_PERIOD_MS), (unsigned long)kDebounceMs, events ? unsigned(uxQueueMessagesWaiting(events)) : 0,
        (unsigned long)dropped, (unsigned long)count, (unsigned long)maximum);
    Serial.printf("keyboard driver=%s fifo_events=%lu fifo_overflows=%lu i2c_errors=%lu\n", controller ? "TCA8418" : "gpio-matrix",
        (unsigned long)fifo, (unsigned long)overruns, (unsigned long)errors);
}
} // namespace keyboard
