// Usage statistics: activations (Off -> Manual/Auto) and run time, for the
// last 24 hours, the last 30 days and all time. Stored in NVS.
//
// The device has no clock: the app sends the time on connect (setTime). Until
// then only the all-time totals count; the windows need wall-clock buckets.
#pragma once
#include <stdint.h>

namespace stats {

struct __attribute__((packed)) Summary {   // also the BLE wire format, 24 bytes
  uint32_t dayCount, daySec;
  uint32_t monthCount, monthSec;
  uint32_t allCount, allSec;
};

enum Which : uint8_t { DAY = 0, MONTH = 1, ALL = 2 };

void    begin();
void    setTime(uint32_t epochS);
bool    timeKnown();
void    onStart();                            // an activation
void    tick(bool running, uint32_t nowMs);   // call every loop
void    flush();                              // save now (on stop)
void    reset(Which which);
Summary summary();
bool    changed();                            // since the last call - time to publish

}  // namespace stats
