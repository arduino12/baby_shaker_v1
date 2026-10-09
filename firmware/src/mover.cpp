#include "mover.h"

#include <math.h>

void Mover::reset(float posDeg) {
  m_pos = m_target = posDeg;
  m_vel = 0;
  m_state = REST;
}

void Mover::setTarget(float deg, uint32_t nowMs) {
  m_target = deg;
  if (m_params.speed == 0 || m_params.accel == 0) return;   // "don't move"
  if (m_state == REST) {
    m_seg = Motion::plan(m_pos, deg, m_params.profile, m_params.speed, m_params.accel);
    m_t0 = m_lastMs = nowMs;
    m_state = m_seg.T > 0 ? SEGMENT : REST;
  } else {
    m_state = FOLLOW;   // keep the current velocity; update() steers to the new target
  }
}

float Mover::update(uint32_t nowMs) {
  const float dt = (nowMs - m_lastMs) / 1000.0f;
  m_lastMs = nowMs;
  if (m_state == SEGMENT) {
    const float t = (nowMs - m_t0) / 1000.0f;
    const float p = Motion::sample(m_seg, t);
    if (dt > 0) m_vel = (p - m_pos) / dt;
    m_pos = p;
    if (t >= m_seg.T) reset(m_pos);
  } else if (m_state == FOLLOW) {
    // Trapezoidal follower: brake in time to stop on the target.
    const float A = m_params.accel, V = m_params.speed, d = m_target - m_pos;
    float vWant = sqrtf(2 * A * fabsf(d));
    if (vWant > V) vWant = V;
    if (d < 0) vWant = -vWant;
    const float dvMax = A * dt;
    m_vel += fmaxf(-dvMax, fminf(dvMax, vWant - m_vel));
    m_pos += m_vel * dt;
    if (fabsf(m_target - m_pos) < 0.3f && fabsf(m_vel) <= dvMax * 2) reset(m_target);
  }
  return m_pos;
}
