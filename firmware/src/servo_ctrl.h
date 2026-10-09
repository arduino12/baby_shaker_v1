// Servo output (LEDC PWM + power switch) and pot feedback.
//
// The rule this module enforces: a pulse is only ever sent for an angle that
// is either where the pot says the arm is, or the next step of a planned
// motion from there. Pulses never start at a remembered or default angle
// while the arm could be somewhere else.
#pragma once
#include "types.h"

namespace servo {

void  begin(const PotCal &cal, float lastKnownDeg);

// Start pulses without moving the arm: measure first (see SERVO_POWER_SWITCHED
// in config.h), then pulse at that angle. Returns it. Already running: returns
// the current command unchanged.
float start();
// Stop pulses at a frame boundary (no truncated pulse), then cut power.
// trustPot: remember the measured angle (not after a stall - the pot may be
// what failed).
void  stop(bool trustPot = true);
bool  running();

void  write(float deg);           // clamped to 0..SERVO_MAX_DEG; logs steps > STEP_WARN_DEG
float written();                  // last angle sent (or remembered while stopped)

bool  feedbackValid();            // calibrated and the pot is powered
void  sample();                   // call once per control tick: feeds the pot median filter
bool  measure(float &deg);        // pot angle if valid and plausible
float read();                     // measured if possible, else written()
// The arm is where it was told to be (pot within SETTLE_TOL_DEG), or there is
// no feedback to tell.
bool  atTarget();
const PotCal &calibration();

// Call every control tick while running; true once the motor is judged stalled.
bool  stalled(uint32_t nowMs);
float stallModel();               // where the slow reference servo is (diagnostics)

// Sweeps 0..max, records the pot table, noise and top speed. Blocks ~20 s.
// Gentle ramps to the start and back; the two timing moves are full speed by
// definition. Returns the new cal (valid = 0 if the pot did not track the PWM)
// and adopts it when valid.
PotCal calibrate();

uint16_t readVbatMv();

}  // namespace servo
