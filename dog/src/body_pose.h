#pragma once
// Whole-body pose -> per-leg heights. Signs follow ROS REP-103 (x forward, y left, z up, right-hand
// rule), so an IMU could use the same convention:
//   + pitch = nose down, + roll = right side down, both in degrees.
//   h_leg = height - x_hip * sin(pitch) + y_foot * sin(roll)
struct BodyPose {
  float height = 0;   // mm, hip axis to floor at the body centre
  float pitch = 0;
  float roll = 0;
};

// Leg heights for a pose, in leg order (FL, FR, RL, RR).
void legHeights(const BodyPose& pose, float out[4]);

// True if every foot can reach this pose: within the leg height limits, solvable by legIK, and with
// no servo past its limit.
bool poseReachable(const BodyPose& pose);

// The closest pose this hardware can hold. Limits height, pitch and roll to config.h, then scales
// pitch and roll down together until every foot can reach. Never fails: it ends at a level body.
BodyPose fitPose(const BodyPose& wanted);
