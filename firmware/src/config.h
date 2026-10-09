// Baby Shaker V1 - hardware and tuning constants.
#pragma once
#include <stdint.h>

// ---------------------------------------------------------------- pins
// ESP32-C3: ADC1 is on GPIO0..4 only. Avoid strapping pins 2/8/9.
constexpr int PIN_SERVO_POS = 0;   // servo pot wiper, through POT_DIVIDER
constexpr int PIN_VBAT      = 1;   // USB 5 V rail, through VBAT_DIVIDER
constexpr int PIN_SERVO_PWM = 5;   // servo signal
constexpr int PIN_SERVO_EN  = 6;   // N-MOS gate, switches servo GND (high = on)
constexpr int PIN_LED       = 7;   // status LED anode (high = on)

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

// ---------------------------------------------------------------- timing
constexpr uint32_t CONTROL_PERIOD_MS  = 20;        // 50 Hz, same as the servo frame
constexpr uint32_t MANUAL_TIMEOUT_MS  = 60UL * 1000;
constexpr uint32_t STATUS_PERIOD_MS   = 1000;
constexpr uint32_t SETTINGS_SAVE_MS   = 3000;      // debounce NVS writes from slider drags

// ---------------------------------------------------------------- BLE
#define BLE_NAME_PREFIX   "Baby Shaker "
#define BLE_SVC_UUID      "8f1d0001-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_MODE_UUID     "8f1d0002-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_POS_UUID      "8f1d0003-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_AUTO_UUID     "8f1d0004-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_STATUS_UUID   "8f1d0005-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
#define BLE_CMD_UUID      "8f1d0006-5b7a-4c2e-9d3b-6a1f2e3c4b5a"
