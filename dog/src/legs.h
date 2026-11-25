#pragma once
#include <Arduino.h>
#include <Adafruit_PWMServoDriver.h>
#include <Preferences.h>
#include "config.h"
#include "leg_ik.h"

// ----------------------------------------------------------------------
//  Servo layer: calibration (offset + direction per channel, saved in flash)
// ----------------------------------------------------------------------
class Servos {
 public:
  bool begin();
  void writeDeg(uint8_t ch, float deg);      // raw servo angle, offset/dir NOT applied
  void writeJoint(uint8_t ch, float jointDeg); // joint angle -> servo angle with calibration
  void relax();                               // stop all pulses (servos go limp)
  void relax(uint8_t ch);
  bool isRelaxed() const { return relaxed_; }

  float offset[8] = {0};
  int8_t dir[8];
  void loadCal();
  void saveCal();
  void resetCal();
  void printCal(Stream& s);
  float lastDeg[8] = {90, 90, 90, 90, 90, 90, 90, 90};

 private:
  Adafruit_PWMServoDriver pca_{PCA_ADDR};
  Preferences prefs_;
  bool relaxed_ = true;
};

class Legs {
 public:
  explicit Legs(Servos& s) : s_(s) {}
  // Move one foot. Returns false (and does nothing) if the point is out of reach.
  bool setFoot(uint8_t leg, float x, float h);
  float footX[4] = {0}, footH[4] = {0};
 private:
  Servos& s_;
};
