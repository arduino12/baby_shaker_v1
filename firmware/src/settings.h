// Persistent settings in NVS (Preferences).
#pragma once
#include "types.h"

namespace settings {
void    begin();
void    loadSlots(AutoParams (&slots)[SLOT_COUNT]);
void    saveSlots(const AutoParams (&slots)[SLOT_COUNT]);
uint8_t loadActiveSlot();
void    saveActiveSlot(uint8_t slot);   // writes only if changed
PotCal  loadPotCal();
void    savePotCal(const PotCal &c);
ManualParams loadManual();
void    saveManual(const ManualParams &p);
float   loadLastPos(float fallback);
void    saveLastPos(float deg);           // writes only if it moved > 0.5 deg
}  // namespace settings
