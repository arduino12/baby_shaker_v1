// Auto-mode motion generator: swings back and forth around mid-range with
// the selected velocity profile, holding at each end.
#pragma once
#include "types.h"

class Motion {
 public:
  void  start(float fromDeg, const AutoParams &p, uint32_t nowMs);
  void  setParams(const AutoParams &p) { m_params = p; }   // applied from the next stroke
  float update(uint32_t nowMs);                            // returns position setpoint

 private:
  struct Segment {
    float from = 0, dir = 1, dist = 0;
    float T = 0;          // total move time, s
    float vp = 0, ta = 0, tc = 0;  // trapezoid / S-curve: peak vel, ramp time, cruise time
    uint8_t profile = 0;
  };

  void  beginStroke(float fromDeg, uint32_t nowMs);
  float endpoint(bool high) const;
  static Segment plan(float from, float to, const AutoParams &p);
  static float   sample(const Segment &s, float t);

  AutoParams m_params{};
  Segment    m_seg;
  bool       m_holding = false;
  bool       m_approach = false;   // first move from wherever the arm was: eased in
  bool       m_towardHigh = true;
  uint32_t   m_t0 = 0;
  float      m_pos = 0;
};
