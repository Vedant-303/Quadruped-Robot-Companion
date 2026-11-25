#include "legs.h"
#include <Wire.h>
#include <math.h>

static float usFromDeg(float deg) {
  return SERVO_US_MIN + (SERVO_US_MAX - SERVO_US_MIN) * deg / 180.0f;
}

bool Servos::begin() {
  Wire.beginTransmission(PCA_ADDR);
  if (Wire.endTransmission() != 0) return false;
  pca_.begin();
  pca_.setOscillatorFrequency(27000000);
  pca_.setPWMFreq(50);
  delay(10);
  for (int i = 0; i < 8; i++) dir[i] = DEFAULT_DIR[i];
  loadCal();
  relax();
  return true;
}

void Servos::writeDeg(uint8_t ch, float deg) {
  if (ch >= 8 || !limitServoDeg(deg)) return;
  lastDeg[ch] = deg;
  pca_.writeMicroseconds(ch, (uint16_t)usFromDeg(deg));
  relaxed_ = false;
}

void Servos::writeJoint(uint8_t ch, float jointDeg) {
  writeDeg(ch, 90.0f + dir[ch] * jointDeg + offset[ch]);
}

void Servos::relax(uint8_t ch) { pca_.setPWM(ch, 0, 4096); }   // full off
void Servos::relax() {
  for (uint8_t ch = 0; ch < 8; ch++) relax(ch);
  relaxed_ = true;
}

void Servos::loadCal() {
  prefs_.begin("deskdog", true);
  for (int i = 0; i < 8; i++) {
    char k[8];
    snprintf(k, sizeof k, "off%d", i); offset[i] = prefs_.getFloat(k, 0.0f);
    snprintf(k, sizeof k, "dir%d", i); dir[i] = prefs_.getChar(k, DEFAULT_DIR[i]);
  }
  prefs_.end();
}

void Servos::saveCal() {
  prefs_.begin("deskdog", false);
  for (int i = 0; i < 8; i++) {
    char k[8];
    snprintf(k, sizeof k, "off%d", i); prefs_.putFloat(k, offset[i]);
    snprintf(k, sizeof k, "dir%d", i); prefs_.putChar(k, dir[i]);
  }
  prefs_.end();
}

void Servos::resetCal() {
  for (int i = 0; i < 8; i++) { offset[i] = 0; dir[i] = DEFAULT_DIR[i]; }
}

void Servos::printCal(Stream& s) {
  s.println("ch  joint                  dir  offset");
  for (int leg = 0; leg < 4; leg++) {
    s.printf("%-2d  %-11s hip        %+d   %+.1f\n", HIP_CH[leg], LEG_NAME[leg], dir[HIP_CH[leg]], offset[HIP_CH[leg]]);
    s.printf("%-2d  %-11s knee       %+d   %+.1f\n", KNEE_CH[leg], LEG_NAME[leg], dir[KNEE_CH[leg]], offset[KNEE_CH[leg]]);
  }
}

// ----------------------------------------------------------------------
bool Legs::setFoot(uint8_t leg, float x, float h) {
  if (leg >= 4) return false;
  JointAngles j = legIK(x, h);
  if (!j.ok) return false;
  s_.writeJoint(HIP_CH[leg], hipJoint(j));
  s_.writeJoint(KNEE_CH[leg], kneeJoint(j));
  footX[leg] = x; footH[leg] = h;
  return true;
}
