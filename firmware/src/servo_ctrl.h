// Servo output (LEDC PWM + power switch) and pot feedback.
#pragma once
#include "types.h"

namespace servo {

void  begin(const PotCal &cal);

// Power the servo and start holding where it physically is: with a valid pot
// calibration it is powered with the signal idle (servo limp), the pot is
// read, and PWM starts at that angle - so nothing jumps. Without calibration
// it starts at the last commanded angle. Returns the start angle.
float enableHere();
void  disable();                  // stop PWM, then cut servo GND
bool  enabled();
void  write(float deg);           // clamped to 0..SERVO_MAX_DEG
float commanded();

// Actual position from the pot when calibrated and powered, else the
// commanded position. With low-side switching the pot floats while off.
float read();
bool  feedbackValid();
const PotCal &calibration();

// Call every control tick while enabled; true once the motor is judged stalled.
bool  stalled(uint32_t nowMs);
float stallModel();               // where the slow reference servo is (diagnostics)

// Sweeps 0..max, records the pot table, noise and top speed. Blocks ~20 s and
// leaves the servo as it found it. Returns the new cal (valid = 0 if the pot
// did not track the PWM, e.g. not wired) and adopts it when valid.
PotCal calibrate();

uint16_t readVbatMv();

}  // namespace servo
