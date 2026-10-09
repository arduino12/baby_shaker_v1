// Persistent settings in NVS (Preferences).
#pragma once
#include "types.h"

namespace settings {
void begin();
AutoParams loadAuto();
void       saveAuto(const AutoParams &p);   // writes only if changed
PotCal     loadPotCal();
void       savePotCal(const PotCal &c);
}  // namespace settings
