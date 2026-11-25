#pragma once
// The emotions: how each mood changes the dog's body. Tune them in this one table.
#include <math.h>
#include <stdint.h>

#include "config.h"

// Values are for intensity 1 and are scaled by the intensity, so 0 is a plain stand.
struct MoodStyle {
  const char* name;
  float height, pitch, roll;          // mm added to the stand; degrees (+ nose down, + right side down)
  float bounceMm, bounceMs;           // up and down: size and period
  float wiggleDeg, wiggleMs;          // side to side: size and period
  float periodScale, walkHeight;      // all a mood may change while walking
  bool stepsBack;                     // take two small steps back first
};

constexpr int MOOD_COUNT = 10;
constexpr MoodStyle MOODS[MOOD_COUNT] = {
  // name       height pitch   roll  bounce         wiggle         walking        back
  {"idle",      0.0f,   0.0f, 0.0f, 1.5f, 4000.0f, 0.0f,   0.0f, 1.00f,  0.0f, false},
  {"happy",     6.0f,   0.0f, 0.0f, 3.0f,  800.0f, 0.0f,   0.0f, 0.85f,  3.0f, false},
  {"excited",   6.0f,   0.0f, 0.0f, 4.0f,  500.0f, 3.0f, 600.0f, 0.75f,  3.0f, false},
  {"sad",     -17.0f,   8.0f, 0.0f, 0.0f,    0.0f, 0.0f,   0.0f, 1.40f, -8.0f, false},
  {"curious",   0.0f,   5.0f, 4.0f, 0.0f,    0.0f, 0.0f,   0.0f, 1.00f,  0.0f, false},
  {"alert",     8.0f,   0.0f, 0.0f, 0.0f,    0.0f, 0.0f,   0.0f, 1.00f,  3.0f, false},
  {"scared",   -4.0f,  -8.0f, 0.0f, 0.0f,    0.0f, 0.0f,   0.0f, 1.00f, -4.0f, true},
  {"sleepy",  -20.0f, -10.0f, 0.0f, 1.0f, 6000.0f, 0.0f,   0.0f, 1.50f, -8.0f, false},
  {"angry",     0.0f,  10.0f, 0.0f, 0.0f,    0.0f, 0.0f,   0.0f, 1.00f,  0.0f, false},
  {"playful",   0.0f,  10.0f, 0.0f, 0.0f,    0.0f, 5.0f, 700.0f, 0.90f,  0.0f, false},
};

constexpr float MOOD_BLEND_S = 0.13f;       // smooths every mood change (about 0.4 s in all)
constexpr float SCARED_STEP_MM = 12.0f;
constexpr float SCARED_STEP_CYCLES = 2.0f;

// Blends toward the current mood, so the dog never snaps from one shape to another.
class Mood {
 public:
  int index = 0;
  float intensity = 0;

  float height = 0, pitch = 0, roll = 0, periodScale = 1, walkHeight = 0;
  float bounce() const { return bounceAmp_ * sinf(bouncePhase_); }
  float wiggle() const { return wiggleAmp_ * sinf(wigglePhase_); }

  void update(float dt) {
    const MoodStyle& m = MOODS[index];
    const float k = 1.0f - expf(-dt / MOOD_BLEND_S);
    height      += k * (intensity * m.height - height);
    pitch       += k * (intensity * m.pitch - pitch);
    roll        += k * (intensity * m.roll - roll);
    walkHeight  += k * (intensity * m.walkHeight - walkHeight);
    periodScale += k * (1.0f + intensity * (m.periodScale - 1.0f) - periodScale);
    bounceAmp_  += k * (intensity * m.bounceMm - bounceAmp_);
    wiggleAmp_  += k * (intensity * m.wiggleDeg - wiggleAmp_);
    advance(bouncePhase_, dt, m.bounceMs);
    advance(wigglePhase_, dt, m.wiggleMs);
  }

 private:
  static void advance(float& phase, float dt, float periodMs) {
    if (periodMs > 0) phase = fmodf(phase + 6.2831853f * dt * 1000.0f / periodMs, 6.2831853f);
  }
  float bounceAmp_ = 0, wiggleAmp_ = 0, bouncePhase_ = 0, wigglePhase_ = 0;
};
