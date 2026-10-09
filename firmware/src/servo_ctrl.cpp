#include "servo_ctrl.h"

#include <Arduino.h>

#include "config.h"

namespace servo {

static bool   s_enabled = false;
static float  s_cmdDeg  = SERVO_MAX_DEG / 2;
static PotCal s_cal     = {};

static uint32_t usToDuty(float us) {
  const float period = 1e6f / SERVO_PWM_HZ;
  return (uint32_t)(us / period * ((1u << SERVO_PWM_BITS) - 1) + 0.5f);
}

static float degToUs(float deg) {
  return SERVO_MIN_US + (SERVO_MAX_US - SERVO_MIN_US) * (deg / SERVO_MAX_DEG);
}

static float clampDeg(float deg) {
  return deg < 0 ? 0 : (deg > SERVO_MAX_DEG ? SERVO_MAX_DEG : deg);
}

static uint16_t readPotMv() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_SERVO_POS);
  return sum / 8;
}

void begin(const PotCal &cal) {
  s_cal = cal;
  pinMode(PIN_SERVO_EN, OUTPUT);
  digitalWrite(PIN_SERVO_EN, LOW);
  pinMode(PIN_SERVO_PWM, OUTPUT);
  digitalWrite(PIN_SERVO_PWM, LOW);
  analogSetAttenuation(ADC_11db);
}

void enable(float startDeg) {
  s_cmdDeg = clampDeg(startDeg);
  if (s_enabled) {
    write(s_cmdDeg);
    return;
  }
  // PWM first so the servo sees a valid pulse from its first powered frame.
  // ledcAttach rejects (not clamps) a bad resolution, and every ledcWrite after
  // that is a silent no-op - so say so loudly.
  if (!ledcAttach(PIN_SERVO_PWM, SERVO_PWM_HZ, SERVO_PWM_BITS))
    Serial.println("[servo] ledcAttach FAILED - no PWM output");
  ledcWrite(PIN_SERVO_PWM, usToDuty(degToUs(s_cmdDeg)));
  digitalWrite(PIN_SERVO_EN, HIGH);
  s_enabled = true;
}

void disable() {
  if (!s_enabled) return;
  // Signal low before cutting GND, so the PWM pin can't back-power the servo
  // electronics through its input while its ground floats.
  ledcDetach(PIN_SERVO_PWM);
  pinMode(PIN_SERVO_PWM, OUTPUT);
  digitalWrite(PIN_SERVO_PWM, LOW);
  digitalWrite(PIN_SERVO_EN, LOW);
  s_enabled = false;
}

bool enabled() { return s_enabled; }

void write(float deg) {
  s_cmdDeg = clampDeg(deg);
  if (s_enabled) ledcWrite(PIN_SERVO_PWM, usToDuty(degToUs(s_cmdDeg)));
}

float commanded() { return s_cmdDeg; }

bool feedbackValid() { return s_cal.valid && s_enabled; }

float read() {
  if (!feedbackValid()) return s_cmdDeg;
  const float mv = readPotMv();
  return clampDeg((mv - s_cal.mvAt0) * SERVO_MAX_DEG / ((float)s_cal.mvAtMax - s_cal.mvAt0));
}

PotCal calibrate() {
  const bool wasEnabled = s_enabled;
  const float wasDeg = s_cmdDeg;
  PotCal cal = {};
  enable(0);
  delay(1500);
  cal.mvAt0 = readPotMv();
  write(SERVO_MAX_DEG);
  delay(1500);
  cal.mvAtMax = readPotMv();
  // Less than ~0.3 V of swing means no pot is connected.
  cal.valid = abs((int)cal.mvAtMax - (int)cal.mvAt0) > 300;
  if (cal.valid) s_cal = cal;
  write(wasDeg);
  if (!wasEnabled) {
    delay(1000);
    disable();
  }
  return cal;
}

uint16_t readVbatMv() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_VBAT);
  return (uint16_t)(sum / 8 * VBAT_DIVIDER);
}

}  // namespace servo
