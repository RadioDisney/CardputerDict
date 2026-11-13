#include "device_profile.h"
#include <cassert>
#include <iostream>

int main() {
    assert(device::kCardputer.sdSck == device::kCardputerAdv.sdSck);
    assert(device::kCardputer.sdMiso == device::kCardputerAdv.sdMiso);
    assert(device::kCardputer.sdMosi == device::kCardputerAdv.sdMosi);
    assert(device::kCardputer.sdCs == device::kCardputerAdv.sdCs);
    bool seen[56] = {};
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 8; ++col) {
            device::TcaEvent press{}, release{};
            const uint8_t number = uint8_t(row * 10 + col + 1);
            assert(device::decodeTca(number | 0x80, press));
            assert(device::decodeTca(number, release));
            assert(press.key < 56 && press.key == release.key && press.pressed && !release.pressed);
            assert(!seen[press.key]);
            seen[press.key] = true;
        }
    }
    for (bool key : seen) assert(key);
    device::TcaEvent invalid{};
    assert(!device::decodeTca(0, invalid));
    assert(!device::decodeTca(9, invalid));
    assert(!device::decodeTca(0x4f, invalid));
    std::cout << "Device profile passed: shared SD pins and bijective 7x8 TCA8418 mapping.\n";
}
