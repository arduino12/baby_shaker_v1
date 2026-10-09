#include "settings.h"

#include <Preferences.h>
#include <string.h>

namespace settings {

static Preferences s_prefs;

void begin() { s_prefs.begin("shaker", false); }

static bool sane(const AutoParams &p) {
  return p.profile <= PROFILE_CUBIC && p.speed <= 360 && p.accel <= 1000 && p.travel >= 10 &&
         p.travel <= 270 && p.holdDs <= 200 && p.durationMin <= 120;
}

void loadSlots(AutoParams (&slots)[SLOT_COUNT]) {
  memcpy(slots, SLOT_DEFAULTS, sizeof(slots));
  if (s_prefs.isKey("slots")) {
    s_prefs.getBytes("slots", slots, sizeof(slots));
    for (uint8_t i = 0; i < SLOT_COUNT; i++)
      if (!sane(slots[i])) slots[i] = SLOT_DEFAULTS[i];
    return;
  }
  // Migrate from the presets firmware: its first preset (or older still, the
  // single "auto" set) becomes Auto 1.
  // getBytes() refuses a buffer smaller than the stored blob, so read it all.
  constexpr size_t kPresetSize = 32 + sizeof(AutoParams);   // name[32] + params
  const size_t len = s_prefs.isKey("presets") ? s_prefs.getBytesLength("presets") : 0;
  if (len >= kPresetSize && len <= 8 * kPresetSize) {
    uint8_t all[8 * kPresetSize];
    if (s_prefs.getBytes("presets", all, len) == len) memcpy(&slots[0], all + 32, sizeof(AutoParams));
  } else if (s_prefs.isKey("auto") && s_prefs.getBytesLength("auto") == sizeof(AutoParams)) {
    s_prefs.getBytes("auto", &slots[0], sizeof(AutoParams));
  }
  saveSlots(slots);
  s_prefs.remove("presets");
  s_prefs.remove("auto");
}

void saveSlots(const AutoParams (&slots)[SLOT_COUNT]) { s_prefs.putBytes("slots", slots, sizeof(slots)); }

uint8_t loadActiveSlot() {
  const uint8_t s = s_prefs.getUChar("slot", 0);
  return s < SLOT_COUNT ? s : 0;
}

void saveActiveSlot(uint8_t slot) {
  if (loadActiveSlot() != slot) s_prefs.putUChar("slot", slot);
}

PotCal loadPotCal() {
  PotCal c = {};
  if (s_prefs.isKey("potcal") && s_prefs.getBytesLength("potcal") == sizeof(c))
    s_prefs.getBytes("potcal", &c, sizeof(c));
  return c;
}

void savePotCal(const PotCal &c) { s_prefs.putBytes("potcal", &c, sizeof(c)); }

}  // namespace settings
