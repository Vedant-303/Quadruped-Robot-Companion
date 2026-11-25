#pragma once
// Tricks: short lists of body poses, played one after another.
#include <stdint.h>

struct Key {
  float height, pitch, roll;   // mm; degrees (+ nose down, + right side down)
  float shift;                 // mm the body moves back (feet slide forward)
  float lift;                  // mm the front-left paw is lifted
  uint16_t ms;                 // how long to hold this key
};

struct Trick { const char* name; const Key* keys; uint8_t count; };

constexpr Key BOW[]     = {{66, 10, 0, 0, 0, 2000}, {72, 0, 0, 0, 0, 500}};
constexpr Key STRETCH[] = {{66, 10, 0, 0, 0, 1400}, {66, -10, 0, 0, 0, 1400}, {72, 0, 0, 0, 0, 500}};
constexpr Key PUSHUP[]  = {{50, 0, 0, 0, 0, 600}, {80, 0, 0, 0, 0, 600}, {50, 0, 0, 0, 0, 600},
                           {80, 0, 0, 0, 0, 600}, {50, 0, 0, 0, 0, 600}, {72, 0, 0, 0, 0, 500}};
constexpr Key WIGGLE[]  = {{72, 0, 6, 0, 0, 300}, {72, 0, -6, 0, 0, 300}, {72, 0, 6, 0, 0, 300},
                           {72, 0, -6, 0, 0, 300}, {72, 0, 6, 0, 0, 300}, {72, 0, 0, 0, 0, 300}};
constexpr Key DANCE[]   = {{78, 0, 5, 0, 0, 350}, {62, 0, -5, 0, 0, 350}, {78, 0, -5, 0, 0, 350}, {62, 0, 5, 0, 0, 350},
                           {78, 5, 0, 0, 0, 350}, {62, -5, 0, 0, 0, 350}, {78, 5, 0, 0, 0, 350}, {72, 0, 0, 0, 0, 400}};
// The body moves back first, so the weight is over the other three legs before the paw lifts.
constexpr Key PAW[]     = {{66, 0, 0, 14, 0, 700}, {66, 0, 0, 14, 24, 600}, {66, 0, 0, 14, 12, 250},
                           {66, 0, 0, 14, 24, 250}, {66, 0, 0, 14, 12, 250}, {66, 0, 0, 14, 24, 600},
                           {66, 0, 0, 14, 0, 600}, {72, 0, 0, 0, 0, 500}};

#define TRICK(name, keys) {name, keys, sizeof(keys) / sizeof(keys[0])}
constexpr Trick TRICKS[] = {
  TRICK("bow", BOW), TRICK("stretch", STRETCH), TRICK("pushup", PUSHUP),
  TRICK("wiggle", WIGGLE), TRICK("dance", DANCE), TRICK("paw", PAW),
};
