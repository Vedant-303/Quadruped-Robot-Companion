#pragma once
// ======================================================================
//  DeskDog basic firmware: all the numbers you may need to change
// ======================================================================
#ifdef ARDUINO
#include <Arduino.h>
#else
#include <stdint.h>   // the native unit tests build without Arduino
#endif

// ---------- pins (see the wiring diagram) ----------
constexpr uint8_t PIN_SDA     = 21;
constexpr uint8_t PIN_SCL     = 22;
constexpr uint8_t PIN_JOY_X   = 34;   // VRx  (left / right = turn)
constexpr uint8_t PIN_JOY_Y   = 35;   // VRy  (up / down = forward / back)
constexpr uint8_t PIN_JOY_BTN = 32;   // SW   (press = toggle stand / sit)
constexpr bool    JOYSTICK_ENABLED = true;   // set false if the joystick isn't wired

// ---------- PCA9685 / servos ----------
constexpr uint8_t PCA_ADDR   = 0x40;
constexpr int     SERVO_US_MIN = 500;   // pulse for 0°   (MG90S)
constexpr int     SERVO_US_MAX = 2400;  // pulse for 180°
constexpr float   SERVO_LIMIT_LO = 5;   // never command beyond these
constexpr float   SERVO_LIMIT_HI = 175;

// ---------- legs ----------
// Leg order everywhere: 0 = front-left, 1 = front-right, 2 = rear-left, 3 = rear-right
enum Leg : uint8_t { FL = 0, FR = 1, RL = 2, RR = 3 };
constexpr const char* LEG_NAME[4] = {"front-left", "front-right", "rear-left", "rear-right"};

// PCA9685 channel for each leg's hip (shoulder) and knee
constexpr uint8_t HIP_CH[4]  = {0, 2, 4, 6};
constexpr uint8_t KNEE_CH[4] = {1, 3, 5, 7};

// Servo direction: +1 or -1. The right-side servos are mounted mirror-image,
// so they usually turn the opposite way. Check with the "test" command and
// flip with "dir <ch> -1" if a joint moves the wrong way.
constexpr int8_t DEFAULT_DIR[8] = {
  +1, +1,   // ch0 FL hip, ch1 FL knee
  -1, -1,   // ch2 FR hip, ch3 FR knee
  +1, +1,   // ch4 RL hip, ch5 RL knee
  -1, -1,   // ch6 RR hip, ch7 RR knee
};

// Geometry (mm), matches the printed thigh and shin
constexpr float L_THIGH = 46.0f;   // hip axis -> knee axis
constexpr float L_SHIN  = 48.0f;   // knee axis -> foot centre

// Body, from the v0.3 printed model. Pitch uses the hips and roll uses the feet.
constexpr float HIP_X  = 44.0f;   // hips sit this far fore and aft of the body centre (mm)
constexpr float FOOT_Y = 77.5f;   // foot pads sit this far either side of the centreline (mm)

// Horn fitting reference: what the leg looks like with every servo at 90° ("zero").
// It is the standing pose, so fit the horns with the dog standing, feet under the hips:
//   hip  90° = thigh tilted 41° from vertical, knee toward the tail
//   knee 90° = shin bent KNEE_NEUTRAL_BEND degrees from straight
constexpr float HIP_NEUTRAL_DEG   = -41.0f;
constexpr float KNEE_NEUTRAL_BEND = 80.0f;

// true  = knees point toward the tail (like the 3D model, "<<" from the left side)
// false = knees point toward the head
constexpr bool KNEES_BACKWARD = true;

// ---------- poses and gait defaults (all changeable from the serial console) ----------
constexpr float STAND_HEIGHT = 72.0f;   // hip axis to floor while standing (mm)
constexpr float SIT_HEIGHT   = 45.0f;   // lying down
constexpr float FOOT_X_FRONT = 4.0f;    // foot position fore/aft of the hip (mm, + = forward)
constexpr float FOOT_X_REAR  = -4.0f;

constexpr float GAIT_PERIOD_MS = 600.0f;  // one full trot cycle
constexpr float STEP_HEIGHT    = 16.0f;   // foot lift during swing (mm)
constexpr float MAX_STRIDE     = 28.0f;   // max foot travel per half cycle (mm)
constexpr float MAX_TURN       = 14.0f;   // stride difference left vs right for turning (mm)

// What this hardware can do. Wider values from the protocol are clamped to these.
constexpr float STAND_HEIGHT_MIN   = 40.0f;
constexpr float STAND_HEIGHT_MAX   = 88.0f;
constexpr float GAIT_PERIOD_MIN_MS = 250.0f;
constexpr float GAIT_PERIOD_MAX_MS = 2000.0f;
constexpr float BODY_PITCH_LIMIT   = 10.0f;    // degrees, + = nose down (ROS REP-103)
constexpr float BODY_ROLL_LIMIT    = 8.0f;     // degrees, + = right side down
constexpr float LEG_HEIGHT_MIN     = 30.0f;    // any one leg, when the body is tilted (mm)
constexpr float LEG_HEIGHT_MAX     = 88.0f;
constexpr float WALK_HEIGHT_MIN    = 60.0f;    // a mood can't take walking height beyond these
constexpr float WALK_HEIGHT_MAX    = 80.0f;

// ---------- postures ----------
constexpr float SIT_POSE_HEIGHT = 52.0f;      // SIT: rear down, nose up
constexpr float SIT_POSE_PITCH  = -10.0f;

// ---------- safety ----------
constexpr uint32_t WATCHDOG_MS     = 1000;    // a brain WALK with no WALK or PING for this long: stop and stand
constexpr uint32_t MOOD_TIMEOUT_MS = 10000;   // a MOOD with no refresh for this long starts to fade
constexpr uint32_t MOOD_FADE_MS    = 1000;    // ...and reaches idle after this much longer
constexpr uint32_t JOY_RELEASE_MS  = 1000;    // the brain gets control back this long after the stick is released

constexpr uint32_t CONTROL_DT_MS = 20;    // 50 Hz control loop (matches servo refresh)
constexpr float    POSE_SPEED    = 60.0f; // mm/s when moving between poses
