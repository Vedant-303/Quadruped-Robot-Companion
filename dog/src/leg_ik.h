#pragma once
#include "config.h"

// ----------------------------------------------------------------------
//  Leg kinematics in the leg's side plane, with no Arduino dependency so the native tests run it.
//  x = foot position forward of the hip axis (mm), h = depth below the hip (mm)
// ----------------------------------------------------------------------
struct JointAngles { float hip; float knee; bool ok; };

// The last check before a servo: limits the angle, and returns false for NaN or infinity, which
// must not be sent at all.
bool limitServoDeg(float& deg);

// Inverse kinematics: foot (x, h) -> hip angle (deg from vertical, + = forward)
// and knee bend (deg from straight, + = bent the configured way).
JointAngles legIK(float x, float h);

// Joint angles use one convention: + = rotating the lower part forward (toward the head).
// With knees pointing back, bending the knee swings the shin forward, so bend is +.
// The knee command is measured from the horn reference (KNEE_NEUTRAL_BEND at 90°).
inline float hipJoint(const JointAngles& j) { return j.hip - HIP_NEUTRAL_DEG; }
inline float kneeJoint(const JointAngles& j) { return (KNEES_BACKWARD ? 1.0f : -1.0f) * (j.knee - KNEE_NEUTRAL_BEND); }
