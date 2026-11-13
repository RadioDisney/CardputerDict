#pragma once
#include <cstdint>

namespace device {
enum class Model { Unsupported, Cardputer, CardputerAdv };
struct Profile {
    Model model;
    const char* name;
    const char* keyboard;
    const char* audio;
    int sdSck, sdMiso, sdMosi, sdCs;
};
inline constexpr Profile kCardputer{Model::Cardputer, "Cardputer", "gpio-matrix", "NS4168", 40, 39, 14, 12};
inline constexpr Profile kCardputerAdv{Model::CardputerAdv, "Cardputer-Adv", "TCA8418", "ES8311", 40, 39, 14, 12};
inline constexpr Profile kUnsupported{Model::Unsupported, "Unsupported", "none", "none", -1, -1, -1, -1};

struct TcaEvent {
    uint8_t key = 0;
    bool pressed = false;
};
// TCA8418 uses row*10+column+1, bit7=press. Remap to the common 4x14 key map.
inline bool decodeTca(uint8_t raw, TcaEvent& result) {
    const uint8_t number = raw & 0x7f;
    if (!number) return false;
    const uint8_t row = (number - 1) / 10, column = (number - 1) % 10;
    if (row >= 7 || column >= 8) return false;
    const uint8_t x = row * 2 + (column >= 4 ? 1 : 0);
    const uint8_t y = column % 4;
    result = {uint8_t(y * 14 + x), bool(raw & 0x80)};
    return true;
}
} // namespace device
