// Auto-mode motion generator: swings back and forth around mid-range with
// the selected velocity profile, holding at each end.
#pragma once
#include "types.h"

class Motion {
 public:
  void  start(float fromDeg, const AutoParams &p, uint32_t nowMs);
  void  setParams(const AutoParams &p) { m_params = p; }   // applied from the next stroke
  float update(uint32_t nowMs);                            // returns position setpoint
  // Standing still (holding at an end, or paused): a mode change can apply
  // now without a jerk. While a stroke runs it waits for the stroke to end.
  bool  atRest() const { return m_holding || m_seg.T <= 0; }
  // At rest at the end of the swing where the cycle started (the first end
  // reached after start()): a full there-and-back cycle is complete.
  bool  atCycleEnd() const { return atRest() && (m_seg.T <= 0 || m_towardHigh == m_anchorHigh); }

  // One rest-to-rest move with a velocity profile (also used by Manual).
  struct Segment {
    float from = 0, dir = 1, dist = 0;
    float T = 0;          // total move time, s
    float vp = 0, ta = 0, tc = 0;  // trapezoid / S-curve: peak vel, ramp time, cruise time
    uint8_t profile = 0;
  };
  static Segment plan(float from, float to, uint8_t profile, float speed, float accel);
  static Segment plan(float from, float to, const AutoParams &p) { return plan(from, to, p.profile, p.speed, p.accel); }
  static float   sample(const Segment &s, float t);

 private:
  void  beginStroke(float fromDeg, uint32_t nowMs);
  float endpoint(bool high) const;

  AutoParams m_params{};
  Segment    m_seg;
  bool       m_holding = false;
  bool       m_approach = false;   // first move from wherever the arm was: eased in
  bool       m_towardHigh = true;
  bool       m_anchorHigh = true;    // the end the cycle starts and finishes at
  uint32_t   m_t0 = 0;
  float      m_pos = 0;
};
