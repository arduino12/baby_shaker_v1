// Servo output (LEDC PWM + power switch) and pot feedback.
#pragma once
#include "types.h"

namespace servo {

void  begin(const PotCal &cal);
void  enable(float startDeg);     // power on and hold startDeg
void  disable();                  // stop PWM, then cut servo GND
bool  enabled();
void  write(float deg);           // clamped to 0..SERVO_MAX_DEG
float commanded();

// Actual position from the pot when calibrated and powered, else the
// commanded position. With low-side switching the pot floats while off.
float read();
bool  feedbackValid();

// Sweeps to both ends and records pot mV. Blocks ~3 s. Returns the new cal
// (valid = 0 if the pot did not move, e.g. not wired).
PotCal calibrate();

uint16_t readVbatMv();

}  // namespace servo
