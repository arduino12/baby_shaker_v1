#include "stats.h"

#include <Arduino.h>
#include <Preferences.h>

namespace stats {

constexpr uint8_t  HOURS = 24, DAYS = 31;
constexpr uint32_t SAVE_EVERY_MS = 60000;   // while running; NVS wear is not a concern at this rate

struct __attribute__((packed)) HourBucket { uint32_t hour; uint16_t count; uint16_t sec; };
struct __attribute__((packed)) DayBucket  { uint32_t day;  uint16_t count; uint32_t sec; };
struct __attribute__((packed)) Data {
  uint32_t   allCount, allSec;
  HourBucket hours[HOURS];   // ring, slot = hour % HOURS
  DayBucket  days[DAYS];     // ring, slot = day % DAYS
};

static Preferences s_prefs;
static Data        s_d = {};
static uint32_t    s_epochBase = 0;   // epoch seconds at millis() == 0; 0 = unknown
static uint32_t    s_accMs = 0, s_lastTickMs = 0, s_lastSaveMs = 0;
static bool        s_dirty = false, s_changed = true;

static uint32_t nowEpoch() { return s_epochBase + millis() / 1000; }

static HourBucket &hourBucket() {
  const uint32_t h = nowEpoch() / 3600;
  HourBucket &b = s_d.hours[h % HOURS];
  if (b.hour != h) b = {h, 0, 0};
  return b;
}

static DayBucket &dayBucket() {
  const uint32_t d = nowEpoch() / 86400;
  DayBucket &b = s_d.days[d % DAYS];
  if (b.day != d) b = {d, 0, 0};
  return b;
}

static void save() {
  s_prefs.putBytes("stats", &s_d, sizeof(s_d));
  s_dirty = false;
  s_lastSaveMs = millis();
}

void begin() {
  s_prefs.begin("stats", false);
  if (s_prefs.isKey("stats") && s_prefs.getBytesLength("stats") == sizeof(s_d))
    s_prefs.getBytes("stats", &s_d, sizeof(s_d));
}

void setTime(uint32_t epochS) {
  if (epochS < 1700000000UL) return;   // not a real time
  s_epochBase = epochS - millis() / 1000;
  s_changed = true;
}

bool timeKnown() { return s_epochBase != 0; }

void onStart() {
  s_d.allCount++;
  if (timeKnown()) {
    hourBucket().count++;
    dayBucket().count++;
  }
  save();
  s_changed = true;
}

void tick(bool running, uint32_t nowMs) {
  const uint32_t dt = nowMs - s_lastTickMs;
  s_lastTickMs = nowMs;
  if (!running) return;
  s_accMs += dt;
  while (s_accMs >= 1000) {
    s_accMs -= 1000;
    s_d.allSec++;
    if (timeKnown()) {
      hourBucket().sec++;
      dayBucket().sec++;
    }
    s_dirty = true;
    s_changed = true;
  }
  if (s_dirty && nowMs - s_lastSaveMs >= SAVE_EVERY_MS) save();
}

void flush() {
  if (s_dirty) save();
}

void reset(Which which) {
  if (which == DAY) memset(s_d.hours, 0, sizeof(s_d.hours));
  else if (which == MONTH) memset(s_d.days, 0, sizeof(s_d.days));
  else s_d.allCount = s_d.allSec = 0;
  save();
  s_changed = true;
}

Summary summary() {
  Summary s = {};
  s.allCount = s_d.allCount;
  s.allSec = s_d.allSec;
  if (timeKnown()) {
    const uint32_t h = nowEpoch() / 3600, d = nowEpoch() / 86400;
    for (const HourBucket &b : s_d.hours)
      if (b.hour && h - b.hour < 24) { s.dayCount += b.count; s.daySec += b.sec; }
    for (const DayBucket &b : s_d.days)
      if (b.day && d - b.day < 30) { s.monthCount += b.count; s.monthSec += b.sec; }
  } else {
    s.dayCount = s.daySec = s.monthCount = s.monthSec = 0xFFFFFFFF;   // unknown until time sync
  }
  return s;
}

bool changed() {
  const bool c = s_changed;
  s_changed = false;
  return c;
}

}  // namespace stats
