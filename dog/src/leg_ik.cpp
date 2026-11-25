#include "leg_ik.h"

#include <math.h>

static constexpr float IK_PI = 3.14159265358979f;
static constexpr float IK_RAD_TO_DEG = 180.0f / IK_PI;

static float clampUnit(float v) { return v < -1.0f ? -1.0f : v > 1.0f ? 1.0f : v; }

bool limitServoDeg(float& deg) {
  if (!isfinite(deg)) return false;
  deg = deg < SERVO_LIMIT_LO ? SERVO_LIMIT_LO : deg > SERVO_LIMIT_HI ? SERVO_LIMIT_HI : deg;
  return true;
}

JointAngles legIK(float x, float h) {
  JointAngles r{0, 0, false};
  if (!isfinite(x) || !isfinite(h)) return r;
  float d = sqrtf(x * x + h * h);
  const float reach = L_THIGH + L_SHIN - 0.5f;
  const float minReach = fabsf(L_THIGH - L_SHIN) + 5.0f;
  if (d > reach || d < minReach || h <= 0) return r;
  float phi = atan2f(x, h);                                   // hip->foot line, from vertical
  float a = acosf(clampUnit((L_THIGH * L_THIGH + d * d - L_SHIN * L_SHIN) / (2 * L_THIGH * d)));
  float g = acosf(clampUnit((L_THIGH * L_THIGH + L_SHIN * L_SHIN - d * d) / (2 * L_THIGH * L_SHIN)));
  float bend = IK_PI - g;                                     // 0 = straight leg
  float hip = KNEES_BACKWARD ? (phi - a) : (phi + a);         // thigh tilts back so the knee sits behind the hip-foot line
  r.hip = hip * IK_RAD_TO_DEG;
  r.knee = bend * IK_RAD_TO_DEG;
  r.ok = true;
  return r;
}
