#pragma once
#include "device_profile.h"

namespace device {
const Profile& profile();
bool detect();
bool advKeyboardPresent();
bool advCodecPresent();
void diagnostics();
} // namespace device
