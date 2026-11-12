#pragma once
#include <Arduino.h>
#include <string>

namespace pronunciation {
void begin();
void setPrepareNetwork(void (*callback)());
bool request(const std::string& word, bool sdReady, bool allowOnline = true);
void tick();
void stop();
bool busy();
bool downloading();
bool changed();
const std::string& word();
const std::string& message();
const char* stateName();
bool configured();
std::string savedSsid();
bool saveWifi(const std::string& ssid, const std::string& password);
void volume(int delta);
uint8_t volume();
void diagnostics();
} // namespace pronunciation
