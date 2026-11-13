#include "device_hardware.h"
#include <M5Cardputer.h>

namespace device {
namespace {
const Profile* current = &kUnsupported;
bool keyboardPresent = false, codecPresent = false;
}
const Profile& profile() { return *current; }
bool detect() {
    switch (M5.getBoard()) {
    case m5::board_t::board_M5Cardputer: current = &kCardputer; break;
    case m5::board_t::board_M5CardputerADV: current = &kCardputerAdv; break;
    default: current = &kUnsupported; return false;
    }
    if (current->model == Model::CardputerAdv) {
        keyboardPresent = M5.In_I2C.isEnabled() && M5.In_I2C.scanID(0x34, 100000);
        codecPresent = M5.In_I2C.isEnabled() && M5.In_I2C.scanID(0x18, 100000);
    }
    diagnostics();
    return true;
}
bool advKeyboardPresent() { return keyboardPresent; }
bool advCodecPresent() { return codecPresent; }
void diagnostics() {
    Serial.printf("DEVICE model=%s board_id=%d keyboard=%s audio=%s flash_bytes=%lu sd_sck=%d sd_miso=%d sd_mosi=%d sd_cs=%d tca8418=%d es8311=%d\n",
        current->name, int(M5.getBoard()), current->keyboard, current->audio, (unsigned long)ESP.getFlashChipSize(),
        current->sdSck, current->sdMiso, current->sdMosi, current->sdCs, keyboardPresent, codecPresent);
}
} // namespace device
