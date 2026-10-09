#include "settings.h"

#include <Preferences.h>
#include <string.h>

namespace settings {

static Preferences s_prefs;

void begin() { s_prefs.begin("shaker", false); }

AutoParams loadAuto() {
  AutoParams p = AUTO_DEFAULTS;
  if (s_prefs.getBytesLength("auto") == sizeof(p)) s_prefs.getBytes("auto", &p, sizeof(p));
  return p;
}

void saveAuto(const AutoParams &p) {
  AutoParams old = loadAuto();
  if (memcmp(&old, &p, sizeof(p)) != 0) s_prefs.putBytes("auto", &p, sizeof(p));
}

PotCal loadPotCal() {
  PotCal c = {};
  if (s_prefs.getBytesLength("potcal") == sizeof(c)) s_prefs.getBytes("potcal", &c, sizeof(c));
  return c;
}

void savePotCal(const PotCal &c) { s_prefs.putBytes("potcal", &c, sizeof(c)); }

// Stored as the packed Preset array; the count is implied by the blob length.
Presets loadPresets() {
  Presets p;
  const size_t len = s_prefs.getBytesLength("presets");
  if (len % sizeof(Preset) == 0 && len <= sizeof(p.items)) {
    s_prefs.getBytes("presets", p.items, len);
    p.count = len / sizeof(Preset);
  }
  return p;
}

void savePresets(const Presets &p) {
  if (p.count == 0) s_prefs.remove("presets");
  else s_prefs.putBytes("presets", p.items, p.count * sizeof(Preset));
}

}  // namespace settings
