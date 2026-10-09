// Shared data types. These structs are also the BLE wire format
// (little-endian, packed) - keep app/app.js in sync when changing them.
#pragma once
#include <stdint.h>

enum Mode : uint8_t { MODE_OFF = 0, MODE_MANUAL = 1, MODE_AUTO = 2 };

enum Profile : uint8_t {
  PROFILE_TRAPEZOIDAL = 0,
  PROFILE_SCURVE      = 1,
  PROFILE_SINUSOIDAL  = 2,
  PROFILE_CUBIC       = 3,
};

struct __attribute__((packed)) AutoParams {   // 12 bytes
  uint8_t  profile;      // Profile
  uint8_t  reserved;
  uint16_t speed;        // deg/s, 0..360
  uint16_t accel;        // deg/s^2, 0..1000
  uint16_t travel;       // deg, 10..270
  uint16_t holdDs;       // hold time in 0.1 s, 0..200
  uint16_t durationMin;  // minutes, 0..120 (0 = no limit)
};

struct __attribute__((packed)) Status {       // 10 bytes
  uint8_t  mode;         // Mode
  uint8_t  flags;        // bit0: pot feedback valid
  uint16_t vbatMv;
  uint16_t posDeg10;     // actual (or commanded) position, 0.1 deg
  uint16_t targetDeg10;  // commanded position, 0.1 deg
  uint16_t remainingS;   // auto: seconds until Off (0xFFFF = no limit); manual: until timeout
};

struct __attribute__((packed)) PotCal {
  uint16_t mvAt0;        // ADC-pin mV at 0 deg
  uint16_t mvAtMax;      // ADC-pin mV at SERVO_MAX_DEG
  uint8_t  valid;
};

constexpr AutoParams AUTO_DEFAULTS = {PROFILE_SINUSOIDAL, 0, 90, 300, 60, 5, 30};
