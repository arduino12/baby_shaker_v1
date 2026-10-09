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

struct __attribute__((packed)) Status {       // 11 bytes
  uint8_t  mode;         // Mode
  uint8_t  flags;        // bit0: pot feedback valid, bit1: stopped because the motor stalled, bit2: calibrating
  uint16_t vbatMv;
  uint16_t posDeg10;     // actual (or commanded) position, 0.1 deg
  uint16_t targetDeg10;  // commanded position, 0.1 deg
  uint16_t remainingS;   // auto: seconds until Off (0xFFFF = no limit); manual: until timeout
  uint8_t  slot;         // active auto slot, 0..3
};

// Pot feedback calibration: pot voltage at CAL_POINTS evenly spaced angles
// (0..SERVO_MAX_DEG), plus what the sweep measured about the servo itself.
constexpr uint8_t CAL_POINTS = 11;
struct __attribute__((packed)) PotCal {
  uint8_t  valid;
  uint8_t  reserved;
  uint16_t mv[CAL_POINTS];   // ADC-pin mV at i * SERVO_MAX_DEG / (CAL_POINTS - 1)
  uint16_t maxSpeedDps;      // slowest direction, 10%..90% of a full-range move
  uint16_t noiseMv;          // peak-to-peak reading while holding still
};

constexpr AutoParams AUTO_DEFAULTS = {PROFILE_SINUSOIDAL, 0, 90, 300, 60, 5, 30};

// Auto 1..4: each button has its own saved parameter set.
constexpr uint8_t SLOT_COUNT = 4;
constexpr AutoParams SLOT_DEFAULTS[SLOT_COUNT] = {
  {PROFILE_SINUSOIDAL, 0, 60, 200, 40, 5, 30},    // gentle
  {PROFILE_SINUSOIDAL, 0, 90, 300, 60, 5, 30},    // medium
  {PROFILE_SCURVE, 0, 150, 500, 90, 3, 30},       // strong
  {PROFILE_TRAPEZOIDAL, 0, 120, 400, 80, 10, 30},
};
