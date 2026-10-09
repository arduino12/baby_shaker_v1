// BLE GATT service. Writes arrive on the NimBLE host task and are parked in
// an inbox that the main loop drains with takeInbox().
#pragma once
#include "types.h"

namespace ble {

enum Cmd : uint8_t { CMD_NONE = 0, CMD_CALIBRATE = 1 };

struct Inbox {
  bool       hasMode = false;
  uint8_t    mode = 0;
  bool       hasPos = false;
  uint16_t   posDeg10 = 0;
  bool       hasAuto = false;
  AutoParams autoParams{};
  uint8_t    cmd = CMD_NONE;
  bool       hasPresetOp = false;
  uint8_t    presetOp = 0, presetIndex = 0;
  Preset     preset{};
};

void        begin(const AutoParams &initial, const Presets &presets);
const char *deviceName();
bool        connected();
bool        takeInbox(Inbox &out);   // true if anything arrived
void        publishMode(uint8_t mode);
void        publishAuto(const AutoParams &p);
void        publishStatus(const Status &s);
void        publishPresets(const Presets &p);

}  // namespace ble
