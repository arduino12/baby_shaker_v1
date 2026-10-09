// Non-blocking status LED patterns.
#pragma once
#include <stdint.h>

namespace led {
void begin();
void update(uint32_t nowMs, bool bleConnected);
}  // namespace led
