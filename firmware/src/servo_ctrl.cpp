#include "servo_ctrl.h"

#include <Arduino.h>

#include "config.h"

namespace servo {

static bool   s_running = false;
static float  s_written = SERVO_MAX_DEG / 2;   // last angle sent / remembered
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

// While the servo drives, its motor current rides on the pot's ground and the
// reading spikes by 10-40 deg for single samples. Each reading is spread over
// ~1.5 ms, and the control loop keeps a median of the last 5 (see sample()).
static uint16_t readPotMv(int samples = 16) {
  uint32_t sum = 0;
  for (int i = 0; i < samples; i++) {
    sum += analogReadMilliVolts(PIN_SERVO_POS);
    delayMicroseconds(60);
  }
  return sum / samples;
}

constexpr uint8_t HIST = 5;
static uint16_t s_hist[HIST];
static uint8_t  s_histN = 0, s_histI = 0;

static uint16_t medianMv() {
  uint16_t v[HIST];
  memcpy(v, s_hist, sizeof(v));
  for (int i = 1; i < HIST; i++)
    for (int j = i; j > 0 && v[j - 1] > v[j]; j--) std::swap(v[j - 1], v[j]);
  return v[HIST / 2];
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

// A reading inside the calibrated span (+5 %). With the wiper disconnected or
// the servo unpowered, the pot reads junk - never steer by that.
static bool plausibleMv(uint16_t mv) {
  const int a = s_cal.mv[0], b = s_cal.mv[CAL_POINTS - 1];
  const int lo = min(a, b), hi = max(a, b), margin = (hi - lo) / 20;
  return mv >= lo - margin && mv <= hi + margin;
}

void begin(const PotCal &cal, float lastKnownDeg) {
  s_cal = cal;
  s_written = clampDeg(lastKnownDeg);
  pinMode(PIN_SERVO_EN, OUTPUT);
  digitalWrite(PIN_SERVO_EN, LOW);
  pinMode(PIN_SERVO_PWM, OUTPUT);
  digitalWrite(PIN_SERVO_PWM, LOW);
  analogSetAttenuation(ADC_11db);
}

bool feedbackValid() { return s_cal.valid && (s_running || !SERVO_POWER_SWITCHED); }

void sample() {
  if (!feedbackValid()) {
    s_histN = 0;
    return;
  }
  s_hist[s_histI] = readPotMv();
  s_histI = (s_histI + 1) % HIST;
  if (s_histN < HIST) s_histN++;
}

bool measure(float &deg) {
  if (!feedbackValid()) return false;
  const uint16_t mv = s_histN == HIST ? medianMv() : readPotMv(32);
  if (!plausibleMv(mv)) return false;
  deg = mvToDeg(s_cal, mv);
  return true;
}

float read() {
  float deg;
  return measure(deg) ? deg : s_written;
}

bool atTarget() {
  float deg;
  return !measure(deg) || fabsf(deg - s_written) <= SETTLE_TOL_DEG;
}

static void resetStallModel() {
  s_modelDeg = read();
  s_modelMs = millis();
  s_stallSinceMs = 0;
}

float start() {
  if (s_running) return s_written;
  s_histN = 0;   // fresh, unfiltered reading: the servo is idle
  if (SERVO_POWER_SWITCHED) {
    digitalWrite(PIN_SERVO_EN, HIGH);   // powered, but no pulses yet: the servo stays limp
    s_running = true;                   // (so feedbackValid() trusts the pot from here)
    delay(SERVO_SETTLE_MS);             // its electronics - which power the pot - come up
  }
  float here;
  const bool measured = measure(here);
  if (measured) s_written = here;
  // ledcAttach rejects (not clamps) a bad resolution, and every ledcWrite after
  // that is a silent no-op - so say so loudly.
  if (!ledcAttach(PIN_SERVO_PWM, SERVO_PWM_HZ, SERVO_PWM_BITS))
    Serial.println("[servo] ledcAttach FAILED - no PWM output");
  ledcWrite(PIN_SERVO_PWM, usToDuty(degToUs(s_written)));
  digitalWrite(PIN_SERVO_EN, HIGH);
  s_running = true;
  resetStallModel();
  Serial.printf("[servo] start at %.1f deg (%s)\n", s_written, measured ? "measured" : "last known - no feedback");
  return s_written;
}

void stop(bool trustPot) {
  if (!s_running) return;
  float here;
  if (trustPot && measure(here)) s_written = here;   // remembered for the next start
  s_histN = 0;
  // Duty 0 takes effect at the next frame, so the pulse in flight finishes
  // cleanly - no truncated (short = "go to 0") pulse. Then cut power, then
  // drive the pin low so it can't back-feed the unpowered servo.
  ledcWrite(PIN_SERVO_PWM, 0);
  delay(25);
  digitalWrite(PIN_SERVO_EN, LOW);
  ledcDetach(PIN_SERVO_PWM);
  pinMode(PIN_SERVO_PWM, OUTPUT);
  digitalWrite(PIN_SERVO_PWM, LOW);
  s_running = false;
  Serial.printf("[servo] stop at %.1f deg\n", s_written);
}

bool running() { return s_running; }

void write(float deg) {
  deg = clampDeg(deg);
  if (s_running && fabsf(deg - s_written) > STEP_WARN_DEG)
    Serial.printf("[jump] command %.1f -> %.1f deg in one step\n", s_written, deg);
  s_written = deg;
  if (s_running) ledcWrite(PIN_SERVO_PWM, usToDuty(degToUs(s_written)));
}

float written() { return s_written; }

const PotCal &calibration() { return s_cal; }

bool stalled(uint32_t nowMs) {
  if (!s_running || !feedbackValid() || s_cal.maxSpeedDps == 0) return false;
  // Advance the slow model toward the command.
  const float dt = (nowMs - s_modelMs) / 1000.0f;
  s_modelMs = nowMs;
  const float step = s_cal.maxSpeedDps * STALL_SPEED_FRACTION * dt;
  const float d = s_written - s_modelDeg;
  s_modelDeg += fabsf(d) <= step ? d : (d > 0 ? step : -step);
  // The model never trails real progress: if the servo is closer to the
  // target, restart the model from there. Otherwise a target reversal (e.g.
  // 0 -> 270 -> 0) would leave the model "ahead" and flag a healthy servo.
  const float pos = read();
  if (fabsf(s_written - pos) < fabsf(s_written - s_modelDeg)) s_modelDeg = pos;
  // A healthy servo is at least as close to the target as the slow model.
  const bool behind = fabsf(s_written - pos) > fabsf(s_written - s_modelDeg) + STALL_TOL_DEG;
  if (!behind) {
    s_stallSinceMs = 0;
    return false;
  }
  if (s_stallSinceMs == 0) s_stallSinceMs = nowMs ? nowMs : 1;
  return nowMs - s_stallSinceMs >= STALL_TIME_MS;
}

float stallModel() { return s_modelDeg; }

void resync() {
  s_histN = 0;
  if (s_running) resetStallModel();
}

// Blocking ramp to `to` at `dps` (calibration only).
static void glide(float to, float dps) {
  const float stepDeg = dps * 0.02f;
  while (fabsf(to - s_written) > stepDeg) {
    write(s_written + (to > s_written ? stepDeg : -stepDeg));
    delay(20);
  }
  write(to);
}

// Full-range move timed on the pot: degrees/second between 10% and 90%.
static uint16_t timeMove(const PotCal &c, float from, float to) {
  glide(from, 90);
  delay(800);
  s_written = to;   // deliberate full-speed move: bypass the jump log
  ledcWrite(PIN_SERVO_PWM, usToDuty(degToUs(to)));
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
  delay(800);
  if (!t10 || !t90 || t90 <= t10) return 0;
  return (uint16_t)(fabsf(hi - lo) / ((t90 - t10) / 1e6f) + 0.5f);
}

PotCal calibrate() {
  const bool wasRunning = s_running;
  const float wasDeg = start();   // measured where possible
  PotCal cal = {};

  glide(0, 60);
  delay(1000);

  // 1. Pot voltage at each table angle.
  for (int i = 0; i < CAL_POINTS; i++) {
    glide(i * CAL_STEP_DEG, 90);
    delay(i == 0 ? 300 : 500);
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

  glide(wasDeg, 60);
  delay(500);
  if (!wasRunning) stop();
  else resetStallModel();
  return cal;
}

uint16_t readVbatMv() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogReadMilliVolts(PIN_VBAT);
  return (uint16_t)(sum / 8 * VBAT_DIVIDER);
}

}  // namespace servo
