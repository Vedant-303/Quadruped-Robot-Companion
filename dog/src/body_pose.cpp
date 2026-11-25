#include "body_pose.h"

#include <math.h>

#include "config.h"
#include "leg_ik.h"

static constexpr float DEG_TO_RADIANS = 3.14159265358979f / 180.0f;
static constexpr int SCALE_STEPS = 20;

static float limit(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void legHeights(const BodyPose& pose, float out[4]) {
  const float tilt = sinf(pose.pitch * DEG_TO_RADIANS) * HIP_X;
  const float lean = sinf(pose.roll * DEG_TO_RADIANS) * FOOT_Y;
  out[FL] = pose.height - tilt + lean;
  out[FR] = pose.height - tilt - lean;
  out[RL] = pose.height + tilt + lean;
  out[RR] = pose.height + tilt - lean;
}

bool poseReachable(const BodyPose& pose) {
  float h[4];
  legHeights(pose, h);
  for (int leg = 0; leg < 4; leg++) {
    if (h[leg] < LEG_HEIGHT_MIN || h[leg] > LEG_HEIGHT_MAX) return false;
    JointAngles j = legIK(leg < 2 ? FOOT_X_FRONT : FOOT_X_REAR, h[leg]);
    if (!j.ok) return false;
    const float hip = 90.0f + hipJoint(j), knee = 90.0f + kneeJoint(j);
    if (hip < SERVO_LIMIT_LO || hip > SERVO_LIMIT_HI || knee < SERVO_LIMIT_LO || knee > SERVO_LIMIT_HI) return false;
  }
  return true;
}

BodyPose fitPose(const BodyPose& wanted) {
  BodyPose p;
  p.height = limit(wanted.height, STAND_HEIGHT_MIN, STAND_HEIGHT_MAX);
  const float pitch = limit(wanted.pitch, -BODY_PITCH_LIMIT, BODY_PITCH_LIMIT);
  const float roll = limit(wanted.roll, -BODY_ROLL_LIMIT, BODY_ROLL_LIMIT);
  for (int step = SCALE_STEPS; step > 0; step--) {
    const float k = (float)step / SCALE_STEPS;
    p.pitch = pitch * k;
    p.roll = roll * k;
    if (poseReachable(p)) return p;
  }
  p.pitch = p.roll = 0;
  return p;
}
