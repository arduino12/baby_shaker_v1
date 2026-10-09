// Baby Shaker V1 - rocks a stroller with a servo, controlled over BLE.
//
// Modes:
//   OFF    - servo unpowered, PWM stopped, CPU at 80 MHz, BLE advertising.
//   MANUAL - the app sets the absolute position; back to OFF after 60 s idle.
//   AUTO   - back-and-forth motion; OFF when duration expires. Four buttons
//            (Auto 1..4), each with its own parameter set in NVS. Slider
//            edits apply live; CMD_SAVE_SLOT stores them into the active slot.
//
// Serial commands (115200): off | man <deg> | auto [1-4] | save | cal | status
//   set <profile> <speed> <accel> <travel> <hold x0.1s> <minutes>  (live; 'save' to keep)

#include <Arduino.h>
#include <esp_log.h>

#include "ble_service.h"
#include "config.h"
#include "led.h"
#include "motion.h"
#include "servo_ctrl.h"
#include "settings.h"

static Mode       g_mode = MODE_OFF;
static AutoParams g_auto;
static Motion     g_motion;
static uint32_t   g_modeStartMs = 0;    // auto: duration timer start
static uint32_t   g_lastManualMs = 0;   // manual: last position command
static uint32_t   g_lastStatusMs = 0;
static AutoParams g_slots[SLOT_COUNT];
static uint8_t    g_slot = 0;          // active Auto button

static const char *modeName(Mode m) {
  return m == MODE_OFF ? "OFF" : m == MODE_MANUAL ? "MANUAL" : "AUTO";
}

static uint16_t remainingS(uint32_t now) {
  if (g_mode == MODE_MANUAL) {
    const uint32_t idle = now - g_lastManualMs;
    return idle >= MANUAL_TIMEOUT_MS ? 0 : (MANUAL_TIMEOUT_MS - idle) / 1000;
  }
  if (g_mode != MODE_AUTO) return 0;
  if (g_auto.durationMin == 0) return 0xFFFF;
  const uint32_t total = g_auto.durationMin * 60000UL, el = now - g_modeStartMs;
  return el >= total ? 0 : (total - el + 999) / 1000;
}

static void sendStatus(uint32_t now) {
  Status s;
  s.mode = g_mode;
  s.flags = servo::feedbackValid() ? 1 : 0;
  s.vbatMv = servo::readVbatMv();
  s.posDeg10 = (uint16_t)(servo::read() * 10 + 0.5f);
  s.targetDeg10 = (uint16_t)(servo::commanded() * 10 + 0.5f);
  s.remainingS = remainingS(now);
  s.slot = g_slot;
  ble::publishStatus(s);
  g_lastStatusMs = now;
}

static void setMode(Mode m, uint32_t now, uint8_t slot = ble::NO_SLOT) {
  if (m > MODE_AUTO) return;
  if (m == MODE_AUTO && slot < SLOT_COUNT) {   // pick a button: load its saved set
    g_slot = slot;
    g_auto = g_slots[slot];
    settings::saveActiveSlot(slot);
    ble::publishAuto(g_auto);
  }
  const float here = servo::read();
  g_mode = m;
  g_modeStartMs = now;
  switch (m) {
    case MODE_OFF:
      servo::disable();
      break;
    case MODE_MANUAL:
      g_lastManualMs = now;
      servo::enable(here);
      break;
    case MODE_AUTO:
      servo::enable(here);
      g_motion.start(here, g_auto, now);
      break;
  }
  if (m == MODE_AUTO) Serial.printf("[mode] AUTO %u\n", g_slot + 1);
  else Serial.printf("[mode] %s\n", modeName(m));
  ble::publishMode(m);
  sendStatus(now);
}

static void setManualPos(float deg, uint32_t now) {
  if (g_mode != MODE_MANUAL) setMode(MODE_MANUAL, now);
  g_lastManualMs = now;
  servo::write(deg);
}

static void setAuto(const AutoParams &p, uint32_t now) {
  const bool durationChanged = p.durationMin != g_auto.durationMin;
  g_auto = p;
  if (g_auto.profile > PROFILE_CUBIC) g_auto.profile = PROFILE_SINUSOIDAL;
  g_motion.setParams(g_auto);
  if (durationChanged) g_modeStartMs = now;   // new duration counts from now
  ble::publishAuto(g_auto);
}

static void saveSlot() {
  g_slots[g_slot] = g_auto;
  settings::saveSlots(g_slots);
  Serial.printf("[slot] saved Auto %u\n", g_slot + 1);
}

static void calibrate(uint32_t now) {
  Serial.println("[cal] sweeping 0 -> max ...");
  const PotCal c = servo::calibrate();
  Serial.printf("[cal] 0deg=%u mV  max=%u mV  -> %s\n", c.mvAt0, c.mvAtMax,
                c.valid ? "OK, saved" : "FAILED (no pot swing), feedback disabled");
  if (c.valid) settings::savePotCal(c);
  sendStatus(now);
}

static void handleSerial(uint32_t now) {
  static String line;
  while (Serial.available()) {
    const char ch = Serial.read();
    if (ch != '\n' && ch != '\r') {
      if (line.length() < 40) line += ch;
      continue;
    }
    line.trim();
    if (line == "off") setMode(MODE_OFF, now);
    else if (line.startsWith("man")) setManualPos(line.substring(3).toFloat(), now);
    else if (line.startsWith("auto")) {
      const int n = line.substring(4).toInt();
      setMode(MODE_AUTO, now, n >= 1 && n <= SLOT_COUNT ? n - 1 : g_slot);
    }
    else if (line == "save") saveSlot();
    else if (line.startsWith("set ")) {   // set <profile> <speed> <accel> <travel> <hold x0.1s> <minutes>
      AutoParams p = {};
      unsigned v[6];
      if (sscanf(line.c_str() + 4, "%u %u %u %u %u %u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
        p = {(uint8_t)v[0], 0, (uint16_t)v[1], (uint16_t)v[2], (uint16_t)v[3], (uint16_t)v[4], (uint16_t)v[5]};
        setAuto(p, now);
      }
    }
    else if (line == "cal") calibrate(now);
    else if (line == "status")
      Serial.printf("mode=%s slot=%u pos=%.1f cmd=%.1f vbat=%umV fb=%d ble=%d prof=%u v=%u a=%u trav=%u hold=%.1fs dur=%umin rem=%us\n",
                    modeName(g_mode), g_slot + 1, servo::read(), servo::commanded(), servo::readVbatMv(),
                    servo::feedbackValid(), ble::connected(), g_auto.profile, g_auto.speed,
                    g_auto.accel, g_auto.travel, g_auto.holdDs / 10.0f, g_auto.durationMin,
                    remainingS(now));
    else if (line.length()) Serial.println("? off | man <deg> | auto [1-4] | set p v a t h d | save | cal | status");
    line = "";
  }
}

// ESP-IDF / NimBLE logs default to UART0, which the SuperMini doesn't expose.
static int logToSerial(const char *fmt, va_list args) {
  char buf[256];
  const int n = vsnprintf(buf, sizeof(buf), fmt, args);
  Serial.print(buf);
  return n;
}

void setup() {
  Serial.begin(115200);
  esp_log_set_vprintf(logToSerial);
  // 80 MHz is the lowest clock BLE runs at - plenty for a 50 Hz control loop.
  setCpuFrequencyMhz(80);
  led::begin();
  settings::begin();
  settings::loadSlots(g_slots);
  g_slot = settings::loadActiveSlot();
  g_auto = g_slots[g_slot];
  servo::begin(settings::loadPotCal());
  ble::begin(g_auto);
  Serial.printf("Baby Shaker V1 - %s\n", ble::deviceName());
}

void loop() {
  static uint32_t lastTick = 0;
  const uint32_t now = millis();

  ble::Inbox in;
  if (ble::takeInbox(in)) {
    if (in.hasAuto) setAuto(in.autoParams, now);
    if (in.hasMode) setMode((Mode)in.mode, now, in.slot);
    if (in.hasPos) setManualPos(in.posDeg10 / 10.0f, now);
    if (in.cmd == ble::CMD_SAVE_SLOT) saveSlot();
    if (in.cmd == ble::CMD_CALIBRATE) calibrate(now);
  }
  handleSerial(now);

  if (now - lastTick >= CONTROL_PERIOD_MS) {
    lastTick = now;
    if (g_mode == MODE_MANUAL && now - g_lastManualMs >= MANUAL_TIMEOUT_MS) {
      Serial.println("[mode] manual idle timeout");
      setMode(MODE_OFF, now);
    } else if (g_mode == MODE_AUTO) {
      if (remainingS(now) == 0) {
        Serial.println("[mode] auto duration elapsed");
        setMode(MODE_OFF, now);
      } else {
        servo::write(g_motion.update(now));
      }
    }
  }

  if (now - g_lastStatusMs >= STATUS_PERIOD_MS) sendStatus(now);
  ble::poll();
  led::update(now, ble::connected());

  // Idle in FreeRTOS between ticks; the CPU waits in WFI there.
  delay(g_mode == MODE_OFF ? 10 : 2);
}
