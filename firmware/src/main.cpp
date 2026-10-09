// Baby Shaker V1 - rocks a stroller with a servo, controlled over BLE.
//
// Modes:
//   OFF    - servo unpowered, PWM stopped, CPU at 80 MHz, BLE advertising.
//   MANUAL - the app sets a target angle; the arm travels there with the
//            manual profile/speed/accel. Back to OFF after 60 s idle.
//   AUTO   - back-and-forth motion; OFF when duration expires. Four buttons
//            (Auto 1..4), each with its own parameter set in NVS. Slider
//            edits apply live; CMD_SAVE_SLOT stores them into the active slot.
//
// A mode change that arrives while the arm is moving waits for the current
// stroke to end (reported as "pending" in the status), so nothing jerks.
// Only a stall stops at once.
//
// Serial commands (115200): off | man [deg] | auto [1-4] | save | cal | calinfo |
//   stats [reset 0-2] | time <epoch> | status
//   set <profile> <speed> <accel> <travel> <hold x0.1s> <minutes>  (live; 'save' to keep)

#include <Arduino.h>
#include <esp_log.h>

#include "ble_service.h"
#include "config.h"
#include "led.h"
#include "motion.h"
#include "mover.h"
#include "servo_ctrl.h"
#include "settings.h"
#include "stats.h"

static Mode         g_mode = MODE_OFF;
static AutoParams   g_auto;
static Motion       g_motion;
static ManualParams g_manual;
static Mover        g_mover;
static uint32_t     g_modeStartMs = 0;    // auto: duration timer start
static uint32_t     g_lastManualMs = 0;   // manual: last position command
static uint32_t     g_lastStatusMs = 0;
static AutoParams   g_slots[SLOT_COUNT];
static uint8_t      g_slot = 0;           // active Auto button
static bool         g_stalled = false;    // last stop was a stall (cleared by the next mode change)
static bool         g_calibrating = false;
static uint32_t     g_lastStatsMs = 0;

// A mode request waiting for the arm to come to rest.
static struct {
  bool    active = false;
  Mode    mode = MODE_OFF;
  uint8_t slot = 0;
  bool    hasTarget = false;   // Manual with a position to go to once it starts
  float   target = 0;
} g_pending;

// NVS writes can take tens of ms (more when a flash page gets erased), so they
// never run inside a mode change - they are batched a moment later.
static bool     g_saveDue = false, g_manualSaveDue = false;
static uint32_t g_saveDueMs = 0, g_manualSaveDueMs = 0;

static const char *modeName(Mode m) {
  return m == MODE_OFF ? "OFF" : m == MODE_MANUAL ? "MANUAL" : "AUTO";
}

// What the motion generator gets: speed capped at the servo's measured top
// speed, so the profile never asks for more than the motor can do.
static AutoParams motionParams() {
  AutoParams p = g_auto;
  const PotCal &cal = servo::calibration();
  if (cal.valid && cal.maxSpeedDps && p.speed > cal.maxSpeedDps) p.speed = cal.maxSpeedDps;
  return p;
}

static ManualParams manualParams() {
  ManualParams p = g_manual;
  const PotCal &cal = servo::calibration();
  if (cal.valid && cal.maxSpeedDps && p.speed > cal.maxSpeedDps) p.speed = cal.maxSpeedDps;
  return p;
}

static bool moving() {
  return (g_mode == MODE_AUTO && !g_motion.atRest()) || (g_mode == MODE_MANUAL && !g_mover.atRest());
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
  s.flags = (servo::feedbackValid() ? 1 : 0) | (g_stalled ? 2 : 0) | (g_calibrating ? 4 : 0);
  s.vbatMv = servo::readVbatMv();
  s.posDeg10 = (uint16_t)(servo::read() * 10 + 0.5f);
  s.targetDeg10 = (uint16_t)((g_mode == MODE_MANUAL ? g_mover.target() : servo::commanded()) * 10 + 0.5f);
  s.remainingS = remainingS(now);
  s.slot = g_slot;
  s.pending = g_pending.active ? (uint8_t)(g_pending.mode | g_pending.slot << 4) : 0xFF;
  ble::publishStatus(s);
  g_lastStatusMs = now;
}

static void setMode(Mode m, uint32_t now, uint8_t slot = ble::NO_SLOT, bool stall = false) {
  if (m > MODE_AUTO) return;
  const uint32_t t0 = micros();
  const bool hasTarget = g_pending.hasTarget;
  const float target = g_pending.target;
  g_pending = {};
  if (m == MODE_AUTO && slot < SLOT_COUNT) {   // pick a button: load its saved set
    g_slot = slot;
    g_auto = g_slots[slot];
    ble::publishAuto(g_auto);
  }
  const Mode prev = g_mode;
  g_mode = m;
  g_modeStartMs = now;
  if (m != MODE_OFF) g_stalled = false;
  if (prev == MODE_OFF && m != MODE_OFF) stats::onStart();
  switch (m) {
    case MODE_OFF:
      servo::disable(!stall);   // after a stall the pot may be what failed: don't trust it
      break;
    case MODE_MANUAL:
      g_lastManualMs = now;
      g_mover.setParams(manualParams());
      g_mover.reset(servo::enableHere());   // hold where the arm is; the app's slider follows
      if (hasTarget) g_mover.setTarget(target, millis());
      break;
    case MODE_AUTO:
      g_motion.start(servo::enableHere(), motionParams(), now);
      break;
  }
  g_saveDue = true;
  g_saveDueMs = now;
  ble::publishMode(m);
  sendStatus(now);
  const float ms = (micros() - t0) / 1000.0f;
  if (m == MODE_AUTO) Serial.printf("[mode] AUTO %u (%.1f ms)\n", g_slot + 1, ms);
  else Serial.printf("[mode] %s (%.1f ms)\n", modeName(m), ms);
}

// Mode change from the user: applies now if the arm is at rest, otherwise when
// the current stroke ends. Asking for the mode that is already on cancels a
// pending change.
static void requestMode(Mode m, uint32_t now, uint8_t slot = ble::NO_SLOT) {
  if (m > MODE_AUTO) return;
  if (m == MODE_AUTO && slot >= SLOT_COUNT) slot = g_slot;
  if (m == g_mode && (m != MODE_AUTO || slot == g_slot)) {
    if (g_pending.active) {
      g_pending = {};
      sendStatus(now);
    }
    return;
  }
  if (moving()) {
    g_pending.active = true;
    g_pending.mode = m;
    g_pending.slot = m == MODE_AUTO ? slot : 0;
    Serial.printf("[mode] %s waits for the stroke to end\n", modeName(m));
    sendStatus(now);
    return;
  }
  setMode(m, now, slot);
}

static void setManualPos(float deg, uint32_t now) {
  g_lastManualMs = now;
  if (g_mode == MODE_MANUAL) {
    g_mover.setTarget(deg, now);
    return;
  }
  g_pending.hasTarget = true;   // go there once Manual starts
  g_pending.target = deg;
  requestMode(MODE_MANUAL, now);
}

static void setAuto(const AutoParams &p, uint32_t now) {
  const bool durationChanged = p.durationMin != g_auto.durationMin;
  g_auto = p;
  if (g_auto.profile > PROFILE_CUBIC) g_auto.profile = PROFILE_SINUSOIDAL;
  g_motion.setParams(motionParams());
  if (durationChanged) g_modeStartMs = now;   // new duration counts from now
  ble::publishAuto(g_auto);
}

static void setManualParams(const ManualParams &p, uint32_t now) {
  g_manual = p;
  if (g_manual.profile > PROFILE_CUBIC) g_manual.profile = PROFILE_SINUSOIDAL;
  g_mover.setParams(manualParams());
  ble::publishManual(g_manual);
  g_manualSaveDue = true;   // slider drags: save once they settle
  g_manualSaveDueMs = now;
}

static void saveSlot() {
  g_slots[g_slot] = g_auto;
  settings::saveSlots(g_slots);
  Serial.printf("[slot] saved Auto %u\n", g_slot + 1);
}

static void calibrate(uint32_t now) {
  g_calibrating = true;
  sendStatus(now);   // the app shows "calibrating" - the loop is blocked for ~20 s
  Serial.println("[cal] sweeping 0 -> max, then timing full-range moves (~20 s) ...");
  const PotCal c = servo::calibrate();
  if (c.valid) {
    settings::savePotCal(c);
    g_motion.setParams(motionParams());
    g_mover.setParams(manualParams());
    Serial.printf("[cal] OK, saved: %u..%u mV, noise %u mV, top speed %u deg/s\n",
                  c.mv[0], c.mv[CAL_POINTS - 1], c.noiseMv, c.maxSpeedDps);
  } else {
    Serial.println("[cal] FAILED: pot did not follow the PWM (not wired?) - old calibration kept");
  }
  if (g_mode == MODE_MANUAL) g_mover.reset(servo::commanded());
  g_calibrating = false;
  sendStatus(millis());
}

static void onStall(uint32_t now) {
  Serial.printf("[stall] target %.1f deg, pot %.1f deg, reference %.1f deg - stopping\n",
                servo::commanded(), servo::read(), servo::stallModel());
  setMode(MODE_OFF, now, ble::NO_SLOT, true);   // at once - no waiting for the stroke
  g_stalled = true;
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
    if (line == "off") requestMode(MODE_OFF, now);
    else if (line == "man") requestMode(MODE_MANUAL, now);   // hold where the arm is
    else if (line.startsWith("man ")) setManualPos(line.substring(4).toFloat(), now);
    else if (line.startsWith("auto")) {
      const int n = line.substring(4).toInt();
      requestMode(MODE_AUTO, now, n >= 1 && n <= SLOT_COUNT ? n - 1 : g_slot);
    }
    else if (line == "save") saveSlot();
    else if (line.startsWith("set ")) {   // set <profile> <speed> <accel> <travel> <hold x0.1s> <minutes>
      unsigned v[6];
      if (sscanf(line.c_str() + 4, "%u %u %u %u %u %u", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6)
        setAuto({(uint8_t)v[0], 0, (uint16_t)v[1], (uint16_t)v[2], (uint16_t)v[3], (uint16_t)v[4], (uint16_t)v[5]}, now);
    }
    else if (line.startsWith("mset ")) {   // mset <profile> <speed> <accel>
      unsigned v[3];
      if (sscanf(line.c_str() + 5, "%u %u %u", &v[0], &v[1], &v[2]) == 3)
        setManualParams({(uint8_t)v[0], 0, (uint16_t)v[1], (uint16_t)v[2]}, now);
    }
    else if (line == "cal") calibrate(now);
    else if (line == "stats") {
      const stats::Summary s = stats::summary();
      Serial.printf("stats (time %s): 24h %lu x / %lu s, 30d %lu x / %lu s, all %lu x / %lu s\n",
                    stats::timeKnown() ? "synced" : "unknown", (unsigned long)s.dayCount, (unsigned long)s.daySec,
                    (unsigned long)s.monthCount, (unsigned long)s.monthSec, (unsigned long)s.allCount, (unsigned long)s.allSec);
    }
    else if (line.startsWith("stats reset ")) stats::reset((stats::Which)line.substring(12).toInt());
    else if (line.startsWith("time ")) stats::setTime(strtoul(line.c_str() + 5, nullptr, 10));
    else if (line == "status")
      Serial.printf("mode=%s slot=%u pending=%s pos=%.1f cmd=%.1f vbat=%umV fb=%d ble=%d prof=%u v=%u a=%u trav=%u hold=%.1fs dur=%umin rem=%us manual=%u/%u/%u\n",
                    modeName(g_mode), g_slot + 1, g_pending.active ? modeName(g_pending.mode) : "-",
                    servo::read(), servo::commanded(), servo::readVbatMv(),
                    servo::feedbackValid(), ble::connected(), g_auto.profile, g_auto.speed,
                    g_auto.accel, g_auto.travel, g_auto.holdDs / 10.0f, g_auto.durationMin,
                    remainingS(now), g_manual.profile, g_manual.speed, g_manual.accel);
    else if (line == "calinfo") {
      const PotCal &k = servo::calibration();
      Serial.printf("cal valid=%u top speed=%u deg/s noise=%u mV table(mV):", k.valid, k.maxSpeedDps, k.noiseMv);
      for (int i = 0; i < CAL_POINTS; i++) Serial.printf(" %u", k.mv[i]);
      Serial.println();
    }
    else if (line.length()) Serial.println("? off | man [deg] | auto [1-4] | set p v a t h d | mset p v a | save | cal | calinfo | stats [reset 0-2] | time <epoch> | status");
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
  g_manual = settings::loadManual();
  servo::begin(settings::loadPotCal());
  servo::setLastKnown(settings::loadLastPos(SERVO_MAX_DEG / 2));
  stats::begin();
  ble::begin(g_auto, g_manual);
  Serial.printf("Baby Shaker V1 - %s\n", ble::deviceName());
}

void loop() {
  static uint32_t lastTick = 0;
  const uint32_t now = millis();

  ble::Inbox in;
  if (ble::takeInbox(in)) {
    if (in.hasAuto) setAuto(in.autoParams, now);
    if (in.hasManual) setManualParams(in.manualParams, now);
    if (in.hasMode) requestMode((Mode)in.mode, now, in.slot);
    if (in.hasPos) setManualPos(in.posDeg10 / 10.0f, now);
    if (in.cmd == ble::CMD_SAVE_SLOT) saveSlot();
    if (in.cmd == ble::CMD_CALIBRATE) calibrate(now);
    if (in.cmd == ble::CMD_RESET_STATS) stats::reset((stats::Which)in.cmdArgs[0]);
    if (in.cmd == ble::CMD_SET_TIME)
      stats::setTime(in.cmdArgs[0] | in.cmdArgs[1] << 8 | in.cmdArgs[2] << 16 | (uint32_t)in.cmdArgs[3] << 24);
  }
  handleSerial(now);

  if (now - lastTick >= CONTROL_PERIOD_MS) {
    lastTick = now;
    if (servo::enabled() && servo::stalled(now)) {
      onStall(now);
    } else if (g_pending.active && !moving()) {
      setMode(g_pending.mode, now, g_pending.slot);   // the stroke has ended
    } else if (g_mode == MODE_MANUAL) {
      if (now - g_lastManualMs >= MANUAL_TIMEOUT_MS && !g_pending.active) {
        Serial.println("[mode] manual idle timeout");
        requestMode(MODE_OFF, now);
      }
      servo::write(g_mover.update(now));
    } else if (g_mode == MODE_AUTO) {
      if (remainingS(now) == 0 && !g_pending.active) {
        Serial.println("[mode] auto duration elapsed");
        requestMode(MODE_OFF, now);
      }
      servo::write(g_motion.update(now));
    }
  }

  // Deferred NVS writes, a second after the last mode change / slider drag.
  if (g_saveDue && now - g_saveDueMs >= 1000) {
    g_saveDue = false;
    settings::saveActiveSlot(g_slot);
    if (g_mode == MODE_OFF) settings::saveLastPos(servo::commanded());
    stats::flush();
  }
  if (g_manualSaveDue && now - g_manualSaveDueMs >= 2000) {
    g_manualSaveDue = false;
    settings::saveManual(g_manual);
  }

  if (now - g_lastStatusMs >= STATUS_PERIOD_MS) sendStatus(now);
  ble::poll();
  stats::tick(g_mode != MODE_OFF, now);
  if (now - g_lastStatsMs >= 1000 && stats::changed()) {
    ble::publishStats(stats::summary());
    g_lastStatsMs = now;
  }
  led::update(now, ble::connected());

  // Idle in FreeRTOS between ticks; the CPU waits in WFI there.
  delay(g_mode == MODE_OFF ? 5 : 2);
}
