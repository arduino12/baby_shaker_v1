// BLE GATT service. Writes arrive on the NimBLE host task and are parked in
// an inbox that the main loop drains with takeInbox().
#pragma once
#include "stats.h"
#include "types.h"

namespace ble {

// Command characteristic: u8 command, then its arguments.
enum Cmd : uint8_t {
  CMD_NONE = 0,
  CMD_CALIBRATE = 1,     // no args
  CMD_SAVE_SLOT = 2,     // no args (saves into the active Auto button)
  CMD_RESET_STATS = 3,   // u8 which: 0 = last 24 h, 1 = last 30 days, 2 = all time
  CMD_SET_TIME = 4,      // u32 epoch seconds (the app sends it on connect)
};
constexpr uint8_t NO_SLOT = 0xFF;

struct Inbox {
  bool       hasMode = false;
  uint8_t    mode = 0;
  uint8_t    slot = NO_SLOT;   // with MODE_AUTO: which Auto button
  bool       hasPos = false;
  uint16_t   posDeg10 = 0;
  bool       hasAuto = false;
  AutoParams autoParams{};
  uint8_t    cmd = CMD_NONE;
  uint8_t    cmdArgs[4] = {};
};

void        begin(const AutoParams &initial);
void        poll();                  // call from loop(): keeps advertising, drops stale links
const char *deviceName();
bool        connected();
bool        takeInbox(Inbox &out);   // true if anything arrived
void        publishMode(uint8_t mode);
void        publishAuto(const AutoParams &p);
void        publishStatus(const Status &s);
void        publishStats(const stats::Summary &s);

}  // namespace ble
