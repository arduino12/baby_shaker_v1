#include "motion.h"

#include <math.h>

#include "config.h"

float Motion::endpoint(bool high) const {
  float travel = m_params.travel;
  if (travel > SERVO_MAX_DEG) travel = SERVO_MAX_DEG;
  const float mid = SERVO_MAX_DEG / 2;
  return high ? mid + travel / 2 : mid - travel / 2;
}

// fromDeg is where the arm really is. The first move goes to the NEARER end
// of the swing with speed/accel capped, so starting Auto never races.
void Motion::start(float fromDeg, const AutoParams &p, uint32_t nowMs) {
  m_params = p;
  m_pos = fromDeg;
  m_towardHigh = fabsf(endpoint(true) - fromDeg) <= fabsf(endpoint(false) - fromDeg);
  m_approach = true;
  m_anchorHigh = m_towardHigh;
  beginStroke(fromDeg, nowMs);
}

void Motion::beginStroke(float fromDeg, uint32_t nowMs) {
  AutoParams p = m_params;
  if (m_approach) {
    if (p.speed > APPROACH_SPEED_DPS) p.speed = APPROACH_SPEED_DPS;
    if (p.accel > APPROACH_ACCEL_DPS2) p.accel = APPROACH_ACCEL_DPS2;
  }
  m_seg = plan(fromDeg, endpoint(m_towardHigh), p);
  m_holding = false;
  m_t0 = nowMs;
}

float Motion::update(uint32_t nowMs) {
  // Speed or acceleration of 0 means "don't move"; re-plan once they change.
  if (m_params.speed == 0 || m_params.accel == 0) {
    m_seg = plan(m_pos, m_pos, m_params);
    m_holding = false;
    m_t0 = nowMs;
    return m_pos;
  }
  const float t = (nowMs - m_t0) / 1000.0f;
  if (!m_holding) {
    m_pos = sample(m_seg, t);
    if (t >= m_seg.T) {
      m_holding = true;
      m_t0 = nowMs;
    }
  } else if (t >= m_params.holdDs / 10.0f) {
    m_approach = false;
    m_towardHigh = !m_towardHigh;
    beginStroke(m_pos, nowMs);
  }
  return m_pos;
}

Motion::Segment Motion::plan(float from, float to, uint8_t profile, float speed, float accel) {
  Segment s;
  s.from = from;
  s.dir = to >= from ? 1 : -1;
  s.dist = fabsf(to - from);
  s.profile = profile;
  const float V = speed, A = accel, D = s.dist;
  if (D < 0.01f || V <= 0 || A <= 0) {
    s.T = 0;
    return s;
  }
  switch (profile) {
    case PROFILE_SINUSOIDAL:
      // x = D(1-cos(pi t/T))/2 : vmax = pi D / 2T, amax = pi^2 D / 2T^2
      s.T = fmaxf(M_PI * D / (2 * V), M_PI * sqrtf(D / (2 * A)));
      break;
    case PROFILE_CUBIC:
      // x = D(3u^2 - 2u^3) : vmax = 1.5 D/T, amax = 6 D/T^2
      s.T = fmaxf(1.5f * D / V, sqrtf(6 * D / A));
      break;
    case PROFILE_SCURVE:
    case PROFILE_TRAPEZOIDAL:
    default: {
      // Ramp time: linear velocity ramp (trapezoid) or smoothstep velocity
      // ramp (S-curve, peak accel 1.5 vp/ta). Both cover vp*ta/2 per ramp.
      const float k = profile == PROFILE_SCURVE ? 1.5f : 1.0f;
      s.vp = V;
      s.ta = k * V / A;
      if (s.vp * s.ta > D) {          // never reaches cruise: triangle
        s.vp = sqrtf(D * A / k);
        s.ta = k * s.vp / A;
      }
      s.tc = (D - s.vp * s.ta) / s.vp;
      s.T = 2 * s.ta + s.tc;
    }
  }
  return s;
}

float Motion::sample(const Segment &s, float t) {
  if (s.T <= 0 || t >= s.T) return s.from + s.dir * s.dist;
  if (t <= 0) return s.from;
  float x;
  switch (s.profile) {
    case PROFILE_SINUSOIDAL:
      x = s.dist * (1 - cosf(M_PI * t / s.T)) / 2;
      break;
    case PROFILE_CUBIC: {
      const float u = t / s.T;
      x = s.dist * u * u * (3 - 2 * u);
      break;
    }
    default: {
      const bool scurve = s.profile == PROFILE_SCURVE;
      // Distance covered after time r into a ramp.
      auto ramp = [&](float r) {
        if (scurve) {
          const float u = r / s.ta;
          return s.vp * s.ta * (u * u * u - u * u * u * u / 2);
        }
        return 0.5f * s.vp * r * r / s.ta;
      };
      if (t < s.ta)
        x = ramp(t);
      else if (t < s.ta + s.tc)
        x = s.vp * s.ta / 2 + s.vp * (t - s.ta);
      else
        x = s.dist - ramp(s.T - t);
    }
  }
  return s.from + s.dir * x;
}
