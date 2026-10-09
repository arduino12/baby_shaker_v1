// Manual-mode motion: moves the arm to each new slider target smoothly.
//
// From rest, a new target gets a planned move with the chosen profile (the
// same planner Auto uses). If a target arrives while moving (slider being
// dragged), it switches to an online follower that keeps the current velocity
// and respects the speed/accel limits - no stop-and-go, no jerk.
#pragma once
#include "motion.h"
#include "types.h"

class Mover {
 public:
  void  reset(float posDeg);                      // at rest here (entering Manual)
  void  setParams(const ManualParams &p) { m_params = p; }
  void  setTarget(float deg, uint32_t nowMs);
  float update(uint32_t nowMs);                   // returns position setpoint
  bool  atRest() const { return m_state == REST; }
  float target() const { return m_target; }

 private:
  enum State { REST, SEGMENT, FOLLOW };
  ManualParams    m_params = MANUAL_DEFAULTS;
  State           m_state = REST;
  Motion::Segment m_seg;
  uint32_t        m_t0 = 0, m_lastMs = 0;
  float           m_pos = 0, m_vel = 0, m_target = 0;
};
