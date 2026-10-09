// BLE GATT service. Writes arrive on the NimBLE host task and are parked in
// an inbox that the main loop drains with takeInbox(). OTA chunks are the one
// exception: they go straight to the ota module.
#pragma once
#include "ota.h"
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
  CMD_OTA_BEGIN = 5,     // u32 image size
  CMD_OTA_END = 6,       // no args: verify, switch boot partition, restart
  CMD_OTA_ABORT = 7,     // no args
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
  bool       hasManual = false;
  ManualParams manualParams{};
  uint8_t    cmd = CMD_NONE;
  uint8_t    cmdArgs[4] = {};
};

void        begin(const AutoParams &autoInitial, const ManualParams &manualInitial);
void        poll();                  // call from loop(): keeps advertising, OTA reports
const char *deviceName();
bool        connected();
bool        takeInbox(Inbox &out);   // true if anything arrived
void        publishMode(uint8_t mode);
void        publishAuto(const AutoParams &p);
void        publishManual(const ManualParams &p);
void        publishStatus(const Status &s);
void        publishStats(const stats::Summary &s);
void        publishOta();
void        fastLink();              // shortest connection interval (OTA)

}  // namespace ble
