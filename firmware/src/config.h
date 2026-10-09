// Baby Shaker V1 - hardware and tuning constants.
#pragma once
#include <stdint.h>

// Shown in the app; the app offers an update when the site has a newer one.
#ifndef FW_VERSION   // bench builds may override it
#define FW_VERSION "1.5.0"
#endif

// ---------------------------------------------------------------- pins
// ESP32-C3: ADC1 is on GPIO0..4 only. Avoid strapping pins 2/8/9.
constexpr int PIN_VBAT      = 0;   // USB 5 V rail, through VBAT_DIVIDER
constexpr int PIN_SERVO_EN  = 1;   // N-MOS gate, switches servo GND (high = on)
constexpr int PIN_SERVO_POS = 3;   // servo pot wiper, through POT_DIVIDER
constexpr int PIN_SERVO_PWM = 4;   // servo signal
constexpr int PIN_LED       = 8;   // status LED (builtin) catode (low = on)
constexpr bool LED_ACTIVE_LOW = true;

// ---------------------------------------------------------------- servo
// 0..SERVO_MAX_DEG maps linearly onto SERVO_MIN_US..SERVO_MAX_US.
// Defaults suit a 270 deg servo (e.g. TD-8153MG); for a 180 deg MG996R use
// 180 / 500 / 2500 or whatever its datasheet says.
constexpr float    SERVO_MAX_DEG  = 270.0f;
constexpr uint16_t SERVO_MIN_US   = 500;
constexpr uint16_t SERVO_MAX_US   = 2500;
constexpr uint32_t SERVO_PWM_HZ   = 50;
constexpr uint8_t  SERVO_PWM_BITS = 14;

// Ratio (input / ADC pin). The servo pot swings up to the servo supply (5 V),
// so it needs a divider to stay under the C3's ~3.1 V ADC range.
constexpr float POT_DIVIDER  = 2.0f;
constexpr float VBAT_DIVIDER = 2.0f;   // 100k / 100k

// ---------------------------------------------------------------- stall detection
// Stalled = the pot is STALL_TOL_DEG further from the target than a servo
// moving at STALL_SPEED_FRACTION of its calibrated top speed would be, for
// STALL_TIME_MS in a row. Needs a valid pot calibration ("cal").
constexpr float    STALL_TOL_DEG        = 20.0f;
constexpr float    STALL_SPEED_FRACTION = 0.3f;   // loaded (stroller) and on a sagging supply it is slower
constexpr uint32_t STALL_TIME_MS        = 500;

// ---------------------------------------------------------------- power-up / mode changes
// Is the servo's GND switched by the N-MOS on PIN_SERVO_EN?
//   false: no N-MOS fitted - the servo is always powered, so the pot can be
//          read even in Off; pulses start exactly where the arm is.
//   true:  power on with no pulses (servo limp), wait SERVO_SETTLE_MS for its
//          electronics (they power the pot), read the pot, then start pulses
//          there. Set this once the N-MOS is fitted.
constexpr bool     SERVO_POWER_SWITCHED = false;
constexpr uint32_t SERVO_SETTLE_MS      = 150;

// A mode change waits until the command has finished its motion AND the pot
// says the arm got there (within SETTLE_TOL_DEG) - or SETTLE_TIMEOUT_MS if it
// can't (load, no calibration). Auto additionally finishes its full cycle,
// back to the end of the swing where it started, when AUTO_FINISH_FULL_CYCLE.
constexpr float    SETTLE_TOL_DEG        = 3.0f;
constexpr uint32_t SETTLE_TIMEOUT_MS     = 1500;
constexpr bool     AUTO_FINISH_FULL_CYCLE = true;

// Diagnostics (serial log): a commanded step bigger than this in one control
// tick, or a pot reading moving faster than POT_JUMP_DPS, is logged as a jump.
constexpr float    STEP_WARN_DEG = 5.0f;
constexpr float    POT_JUMP_DPS  = 400.0f;
// Auto's first move, from wherever the arm is to the first end of the swing,
// is capped to these so it eases in instead of racing.
constexpr uint16_t APPROACH_SPEED_DPS  = 60;
constexpr uint16_t APPROACH_ACCEL_DPS2 = 200;

// ---------------------------------------------------------------- timing
constexpr uint32_t CONTROL_PERIOD_MS  = 20;        // 50 Hz, same as the servo frame
constexpr uint32_t MANUAL_TIMEOUT_MS  = 60UL * 1000;
constexpr uint32_t STATUS_PERIOD_MS   = 500;

// ---------------------------------------------------------------- BLE
#define BLE_NAME_PREFIX   "Baby Shaker "
#define BLE_SVC_UUID      "8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_MODE_UUID     "8f1d0002-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_POS_UUID      "8f1d0003-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_AUTO_UUID     "8f1d0004-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_STATUS_UUID   "8f1d0005-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_CMD_UUID      "8f1d0006-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_STATS_UUID    "8f1d0008-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_MANUAL_UUID   "8f1d0009-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_OTA_UUID      "8f1d000a-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_INFO_UUID     "8f1d000b-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
