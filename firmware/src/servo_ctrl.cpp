#include "servo_ctrl.h"

#include <Arduino.h>

#include "config.h"

namespace servo {

static bool   s_enabled = false;
static float  s_cmdDeg  = SERVO_MAX_DEG / 2;
static PotCal s_cal     = {};

// Stall model: a "slow servo" chasing the command; see config.h.
static float    s_modelDeg = 0;
static uint32_t s_modelMs = 0, s_stallSinceMs = 0;

constexpr float CAL_STEP_DEG = SERVO_MAX_DEG / (CAL_POINTS - 1);

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

static uint16_t readPotMv(int samples = 8) {
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) sum += analogReadMilliVolts(PIN_SERVO_POS);
  return sum / samples;
}

// Piecewise-linear inverse of the calibration table. Works whether the pot
// voltage rises or falls with angle.
static float mvToDeg(const PotCal &c, float mv) {
  const bool rising = c.mv[CAL_POINTS - 1] > c.mv[0];
  int i = 0;
  while (i < CAL_POINTS - 2 && (rising ? mv > c.mv[i + 1] : mv < c.mv[i + 1])) i++;
  const float span = (float)c.mv[i + 1] - c.mv[i];
  const float f = span != 0 ? (mv - c.mv[i]) / span : 0;
  return clampDeg((i + f) * CAL_STEP_DEG);
}

void begin(const PotCal &cal) {
  s_cal = cal;
  pinMode(PIN_SERVO_EN, OUTPUT);
  digitalWrite(PIN_SERVO_EN, LOW);
  pinMode(PIN_SERVO_PWM, OUTPUT);
  digitalWrite(PIN_SERVO_PWM, LOW);
  analogSetAttenuation(ADC_11db);
}

static void startPwm(float deg) {
  s_cmdDeg = clampDeg(deg);
  // ledcAttach rejects (not clamps) a bad resolution, and every ledcWrite after
  // that is a silent no-op - so say so loudly.
  if (!ledcAttach(PIN_SERVO_PWM, SERVO_PWM_HZ, SERVO_PWM_BITS))
    Serial.println("[servo] ledcAttach FAILED - no PWM output");
  ledcWrite(PIN_SERVO_PWM, usToDuty(degToUs(s_cmdDeg)));
}

float enableHere() {
  if (s_enabled) {
    write(read());   // already powered: stop where it is now
  } else if (s_cal.valid) {
    digitalWrite(PIN_SERVO_EN, HIGH);   // signal stays low: no pulses, servo stays limp
    delay(60);                          // pot and its divider settle
    startPwm(mvToDeg(s_cal, readPotMv(16)));
  } else {
    startPwm(s_cmdDeg);   // PWM first, so the first powered frame has a pulse
    digitalWrite(PIN_SERVO_EN, HIGH);
  }
  s_enabled = true;
  s_modelDeg = read();
  s_modelMs = millis();
  s_stallSinceMs = 0;
  return s_cmdDeg;
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

const PotCal &calibration() { return s_cal; }

float read() {
  if (!feedbackValid()) return s_cmdDeg;
  return mvToDeg(s_cal, readPotMv());
}

bool stalled(uint32_t nowMs) {
  if (!feedbackValid() || s_cal.maxSpeedDps == 0) return false;
  // Advance the slow model toward the command.
  const float dt = (nowMs - s_modelMs) / 1000.0f;
  s_modelMs = nowMs;
  const float step = s_cal.maxSpeedDps * STALL_SPEED_FRACTION * dt;
  const float d = s_cmdDeg - s_modelDeg;
  s_modelDeg += fabsf(d) <= step ? d : (d > 0 ? step : -step);
  // The model never trails real progress: if the servo is closer to the
  // target, restart the model from there. Otherwise a target reversal (e.g.
  // 0 -> 270 -> 0) would leave the model "ahead" and flag a healthy servo.
  const float pos = read();
  if (fabsf(s_cmdDeg - pos) < fabsf(s_cmdDeg - s_modelDeg)) s_modelDeg = pos;
  // A healthy servo is at least as close to the target as the slow model.
  const bool behind = fabsf(s_cmdDeg - pos) > fabsf(s_cmdDeg - s_modelDeg) + STALL_TOL_DEG;
  if (!behind) {
    s_stallSinceMs = 0;
    return false;
  }
  if (s_stallSinceMs == 0) s_stallSinceMs = nowMs ? nowMs : 1;
  return nowMs - s_stallSinceMs >= STALL_TIME_MS;
}

float stallModel() { return s_modelDeg; }

// Full-range move timed on the pot: degrees/second between 10% and 90%.
static uint16_t timeMove(const PotCal &c, float from, float to) {
  write(from);
  delay(2000);
  write(to);
  const float lo = from + (to - from) * 0.1f, hi = from + (to - from) * 0.9f;
  const bool up = to > from;
  uint32_t t10 = 0, t90 = 0;
  const uint32_t start = micros();
  while (micros() - start < 4000000UL) {
    const float deg = mvToDeg(c, readPotMv(4));
    const uint32_t now = micros();
    if (!t10 && (up ? deg >= lo : deg <= lo)) t10 = now;
    if (!t90 && (up ? deg >= hi : deg <= hi)) { t90 = now; break; }
  }
  if (!t10 || !t90 || t90 <= t10) return 0;
  return (uint16_t)(fabsf(hi - lo) / ((t90 - t10) / 1e6f) + 0.5f);
}

PotCal calibrate() {
  const bool wasEnabled = s_enabled;
  const float wasDeg = s_cmdDeg;
  PotCal cal = {};

  if (!s_enabled) {
    startPwm(0);
    digitalWrite(PIN_SERVO_EN, HIGH);
    s_enabled = true;
  }
  write(0);
  delay(2000);

  // 1. Pot voltage at each table angle.
  for (int i = 0; i < CAL_POINTS; i++) {
    write(i * CAL_STEP_DEG);
    delay(i == 0 ? 0 : 700);
    cal.mv[i] = readPotMv(32);
    Serial.printf("[cal] %5.1f deg -> %4u mV\n", i * CAL_STEP_DEG, cal.mv[i]);
  }

  // 2. Noise while holding still at the far end.
  uint16_t lo = 0xFFFF, hi = 0;
  for (int i = 0; i < 100; i++) {
    const uint16_t v = readPotMv(4);
    lo = min(lo, v);
    hi = max(hi, v);
    delay(2);
  }
  cal.noiseMv = hi - lo;

  // The table must be monotonic with a real swing, else no pot is connected.
  const bool rising = cal.mv[CAL_POINTS - 1] > cal.mv[0];
  bool monotonic = abs((int)cal.mv[CAL_POINTS - 1] - (int)cal.mv[0]) > 300;
  for (int i = 0; monotonic && i < CAL_POINTS - 1; i++)
    monotonic = rising ? cal.mv[i + 1] > cal.mv[i] : cal.mv[i + 1] < cal.mv[i];

  // 3. Top speed, both directions (the slower one counts).
  if (monotonic) {
    const uint16_t up = timeMove(cal, 0, SERVO_MAX_DEG);
    const uint16_t down = timeMove(cal, SERVO_MAX_DEG, 0);
    Serial.printf("[cal] speed up %u deg/s, down %u deg/s\n", up, down);
    cal.maxSpeedDps = (up && down) ? min(up, down) : max(up, down);
    cal.valid = 1;
    s_cal = cal;
  }

  write(wasDeg);
  delay(1500);
  if (!wasEnabled) disable();
  else enableHere();   // re-seed the stall model
  return cal;
}

uint16_t readVbatMv() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_VBAT);
  return (uint16_t)(sum / 8 * VBAT_DIVIDER);
}

}  // namespace servo
