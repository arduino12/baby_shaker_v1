#include "led.h"

#include <Arduino.h>

#include "config.h"

namespace led {

// Alternating on/off durations (ms), starting with "on".
static const uint16_t kIdle[]      = {100, 1900};
static const uint16_t kConnected[] = {100, 100, 100, 700};

static bool     s_connected = false;
static uint8_t  s_step = 0;
static uint32_t s_stepStart = 0;

void begin() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LED_ACTIVE_LOW ? HIGH : LOW);
}

void update(uint32_t nowMs, bool bleConnected) {
  if (bleConnected != s_connected) {   // restart the pattern on change
    s_connected = bleConnected;
    s_step = 0;
    s_stepStart = nowMs;
  }
  const uint16_t *pat = s_connected ? kConnected : kIdle;
  const uint8_t   len = s_connected ? 4 : 2;
  while (nowMs - s_stepStart >= pat[s_step]) {
    s_stepStart += pat[s_step];
    s_step = (s_step + 1) % len;
  }
  const bool on = s_step % 2 == 0;
  digitalWrite(PIN_LED, on != LED_ACTIVE_LOW ? HIGH : LOW);
}

}  // namespace led
