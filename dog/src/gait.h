#pragma once
#include "config.h"
#include "legs.h"

// ----------------------------------------------------------------------
//  Motion: smooth pose changes and a trot gait.
//  Trot = diagonal pairs move together: FL+RR, then FR+RL.
// ----------------------------------------------------------------------
enum class Mode : uint8_t { Relaxed, Pose, Walk };

class Motion {
 public:
  explicit Motion(Legs& legs) : legs_(legs) {}

  void stand()                  { goPose(standHeight); }
  void sit()                    { goPose(sitHeight); }
  void walk(float stride, float turn);   // stride/turn in mm, -MAX..+MAX
  void stop();                            // back to standing
  void relax()                  { mode_ = Mode::Relaxed; }
  void update(float dt);                  // call every CONTROL_DT_MS
  void snapTo(float h);                   // jump straight to a pose (used at power-up)
  void syncFrom(const float* x, const float* h, float fallbackH) {   // continue from where the feet are
    for (uint8_t i = 0; i < 4; i++) { curX_[i] = x[i]; curH_[i] = h[i] > 0 ? h[i] : fallbackH; }
    mode_ = Mode::Pose;
  }

  void goHeight(float h)        { goPose(h); }   // hold a height without changing standHeight
  void setLegHeights(const float h[4]) {         // per-leg targets while in a pose; the glide still applies
    for (uint8_t i = 0; i < 4; i++) targetLegH_[i] = h[i];
  }

  Mode mode() const { return mode_; }
  bool standing() const { return mode_ != Mode::Relaxed && fabsf(targetH_ - standHeight) < 0.5f; }
  float strideCommand() const { return strideCmd_; }
  float turnCommand() const   { return turnCmd_; }   // + = to the dog's right
  float targetHeight() const  { return mode_ == Mode::Walk ? standHeight : targetH_; }

  // tunables (changeable from the console)
  float standHeight = STAND_HEIGHT;
  float sitHeight   = SIT_HEIGHT;
  float periodMs    = GAIT_PERIOD_MS;
  float stepHeight  = STEP_HEIGHT;
  float periodScale = 1.0f;         // set by the mood while walking
  float walkHeightOffset = 0.0f;
  float shiftX = 0.0f;              // in a pose: slides all feet forward, so the body moves back (mm)

 private:
  void goPose(float h);
  float baseX(uint8_t leg) const { return leg < 2 ? FOOT_X_FRONT : FOOT_X_REAR; }
  bool isLeft(uint8_t leg) const { return leg == FL || leg == RL; }

  Legs& legs_;
  Mode mode_ = Mode::Relaxed;
  float curX_[4] = {0}, curH_[4] = {0};   // where each foot is now
  float targetH_ = STAND_HEIGHT;
  float targetLegH_[4] = {STAND_HEIGHT, STAND_HEIGHT, STAND_HEIGHT, STAND_HEIGHT};
  float phase_ = 0;
  float strideCmd_ = 0, turnCmd_ = 0;     // requested
  float stride_ = 0, turn_ = 0;           // ramped
};

inline void Motion::snapTo(float h) {
  for (uint8_t i = 0; i < 4; i++) { curX_[i] = baseX(i); curH_[i] = h; legs_.setFoot(i, curX_[i], curH_[i]); }
  targetH_ = h;
  for (uint8_t i = 0; i < 4; i++) targetLegH_[i] = h;
  mode_ = Mode::Pose;
}

inline void Motion::goPose(float h) {
  targetH_ = constrain(h, 30.0f, L_THIGH + L_SHIN - 4.0f);
  for (uint8_t i = 0; i < 4; i++) targetLegH_[i] = targetH_;
  strideCmd_ = turnCmd_ = stride_ = turn_ = 0;
  mode_ = Mode::Pose;
}

inline void Motion::walk(float stride, float turn) {
  strideCmd_ = constrain(stride, -MAX_STRIDE, MAX_STRIDE);
  turnCmd_   = constrain(turn, -MAX_TURN, MAX_TURN);
  if (mode_ != Mode::Walk) {
    targetH_ = standHeight;
    phase_ = 0;
    mode_ = Mode::Walk;
  }
}

inline void Motion::stop() { goPose(standHeight); }

inline void Motion::update(float dt) {
  if (mode_ == Mode::Relaxed) return;
  const float step = POSE_SPEED * dt;   // mm this tick

  if (mode_ == Mode::Pose) {
    // glide every foot toward (baseX + shiftX, its target height)
    for (uint8_t i = 0; i < 4; i++) {
      float dx = baseX(i) + shiftX - curX_[i], dh = targetLegH_[i] - curH_[i];
      float d = sqrtf(dx * dx + dh * dh);
      if (d > step) { curX_[i] += dx / d * step; curH_[i] += dh / d * step; }
      else          { curX_[i] = baseX(i) + shiftX; curH_[i] = targetLegH_[i]; }
      legs_.setFoot(i, curX_[i], curH_[i]);
    }
    return;
  }

  // ---- Walk ----
  // ramp the commands so starts and stops are gentle
  const float ramp = 80.0f * dt;
  stride_ += constrain(strideCmd_ - stride_, -ramp, ramp);
  turn_   += constrain(turnCmd_ - turn_, -ramp, ramp);

  phase_ += dt * 1000.0f / (periodMs * periodScale);
  const float walkHeight = standHeight + walkHeightOffset;
  if (phase_ >= 1.0f) phase_ -= 1.0f;

  static const float OFFSET[4] = {0.0f, 0.5f, 0.5f, 0.0f};   // FL, FR, RL, RR
  for (uint8_t i = 0; i < 4; i++) {
    float S = stride_ + (isLeft(i) ? turn_ : -turn_);       // this leg's stride
    float p = phase_ + OFFSET[i]; if (p >= 1.0f) p -= 1.0f;
    float x, h;
    if (p < 0.5f) {                       // stance: foot on the desk, sweeping back
      float u = p / 0.5f;
      x = baseX(i) + S / 2 - u * S;
      h = walkHeight;
    } else {                              // swing: lift and bring the foot forward
      float u = (p - 0.5f) / 0.5f;
      x = baseX(i) - S / 2 + S * (1 - cosf(PI * u)) / 2;
      h = walkHeight - stepHeight * sinf(PI * u);
    }
    // blend in from the current pose over a few ticks (first steps after standing)
    curX_[i] += constrain(x - curX_[i], -2 * step, 2 * step);
    curH_[i] += constrain(h - curH_[i], -4 * step, 4 * step);
    legs_.setFoot(i, curX_[i], curH_[i]);
  }
}
