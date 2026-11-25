// ======================================================================
//  DeskDog basic firmware: walking, movements and moods
//  ESP32 DevKit + PCA9685 + 8 x MG90S + joystick. Serial at 115200 baud.
//
//  UPPERCASE commands come from a program (emotion.py): they need keep-alives and give way to the
//  joystick. lowercase commands are typed by you and stay until you change them. Type "help".
// ======================================================================
#include <Arduino.h>
#include <Wire.h>
#include <ctype.h>
#include <string.h>

#include "body_pose.h"
#include "config.h"
#include "gait.h"
#include "legs.h"
#include "moods.h"
#include "tricks.h"

Servos servos;
Legs legs(servos);
Motion motion(legs);
Mood mood;

// ---------------- what the dog is doing ----------------
enum class Posture : uint8_t { Stand, Sit, Lie, Held, Trick };
const char* const POSTURE_NAME[] = {"stand", "sit", "relax", "stand", "stand"};

Posture posture = Posture::Lie;
BodyPose heldPose;                 // from POSE
const Trick* trick = nullptr;
uint8_t trickKey = 0;
uint32_t trickKeyMs = 0;

bool moodMayWake = true;           // from power-up a MOOD may stand the dog up, until you move servos by hand
bool moodExpires = false;
float moodPeak = 0;
uint32_t moodMs = 0;

bool programWalk = false;          // the walk came from a program, so the watchdog is on
uint32_t walkMs = 0;
bool steppingBack = false;         // scared
uint32_t stepBackStartMs = 0;

bool joyDriving = false;
bool joyUsed = false;
uint32_t joyMs = 0;

bool joystickBusy(uint32_t now) { return joyUsed && now - joyMs < JOY_RELEASE_MS; }
bool walking() { return motion.mode() == Mode::Walk; }

// Take over from limp servos without a jump where possible.
void wake() {
  if (servos.isRelaxed()) motion.snapTo(motion.sitHeight);   // the first move: legs jump to the low pose
  else if (motion.mode() == Mode::Relaxed) motion.syncFrom(legs.footX, legs.footH, motion.sitHeight);
}

void setPosture(Posture p) {
  wake();
  moodMayWake = programWalk = steppingBack = joyDriving = false;
  posture = p;
  motion.stand();                  // ends any walk; applyBody() sets the real shape
}

// turn: + = to the dog's left (Motion uses + = right)
void startWalk(float stride, float turn) {
  wake();
  moodMayWake = programWalk = steppingBack = joyDriving = false;
  posture = Posture::Stand;
  motion.walk(stride, -turn);
}

void goLimp() {
  moodMayWake = programWalk = steppingBack = joyDriving = false;
  posture = Posture::Lie;
  motion.relax();
}

void setMood(int index, float intensity, bool expires, uint32_t now) {
  const bool changed = index != mood.index;
  if (changed && steppingBack) setPosture(Posture::Stand);
  mood.index = index;
  mood.intensity = moodPeak = intensity;
  moodExpires = expires;
  moodMs = now;
  if (expires && joystickBusy(now)) return;
  if (motion.mode() == Mode::Relaxed) {
    if (moodMayWake) setPosture(Posture::Stand);
  } else if (changed && MOODS[index].stepsBack && intensity > 0 && posture == Posture::Stand && !walking()) {
    startWalk(-SCARED_STEP_MM * intensity, 0);
    steppingBack = true;
    stepBackStartMs = now;
  }
}

// ---------------- called every 20 ms ----------------
void updateSafety(uint32_t now) {
  if (programWalk && now - walkMs >= WATCHDOG_MS) {
    setPosture(Posture::Stand);
    Serial.println("# watchdog: no WALK or PING, stopping");
  }
  if (steppingBack && now - stepBackStartMs >= SCARED_STEP_CYCLES * motion.periodMs) setPosture(Posture::Stand);
  if (moodExpires) {
    const uint32_t age = now - moodMs;
    if (age >= MOOD_TIMEOUT_MS + MOOD_FADE_MS) { mood.index = 0; mood.intensity = 0; moodExpires = false; }
    else if (age >= MOOD_TIMEOUT_MS) mood.intensity = moodPeak * (1.0f - (float)(age - MOOD_TIMEOUT_MS) / MOOD_FADE_MS);
  }
  if (posture == Posture::Trick && now - trickKeyMs >= trick->keys[trickKey].ms) {
    trickKeyMs = now;
    if (++trickKey >= trick->count) posture = Posture::Stand;
  }
}

// Turns posture and mood into leg heights. While walking, a mood only changes speed and height.
void applyBody() {
  motion.shiftX = 0;
  if (motion.mode() == Mode::Relaxed) return;
  if (walking()) {
    const float p = motion.periodMs;
    motion.periodScale = constrain(p * mood.periodScale, GAIT_PERIOD_MIN_MS, GAIT_PERIOD_MAX_MS) / p;
    const float lowest = WALK_HEIGHT_MIN - motion.standHeight, highest = WALK_HEIGHT_MAX - motion.standHeight;
    motion.walkHeightOffset = constrain(mood.walkHeight, lowest, highest);
    return;
  }
  BodyPose want;
  float lift = 0;
  switch (posture) {
    case Posture::Stand:
      want.height = motion.standHeight + mood.height + mood.bounce();
      want.pitch = mood.pitch;
      want.roll = mood.roll + mood.wiggle();
      break;
    case Posture::Sit:
      want.height = SIT_POSE_HEIGHT + mood.bounce();
      want.pitch = SIT_POSE_PITCH;
      break;
    case Posture::Lie:
      want.height = SIT_HEIGHT + mood.bounce();
      break;
    case Posture::Held:
      want = heldPose;
      break;
    case Posture::Trick: {
      const Key& k = trick->keys[trickKey];
      want.height = k.height;
      want.pitch = k.pitch;
      want.roll = k.roll;
      motion.shiftX = k.shift;
      lift = k.lift;
      break;
    }
  }
  float h[4];
  legHeights(fitPose(want), h);      // fitPose never asks for something the legs can't reach
  h[FL] -= lift;
  motion.setLegHeights(h);
}

// ---------------- protocol commands ----------------
bool inRange(float v, float limit) { return v >= -limit && v <= limit; }   // false for NaN

// Returns the reply, or nullptr if it was already printed. `program` = the command was uppercase.
const char* runCommand(const char* cmd, const char* args, bool program, uint32_t now) {
  const auto is = [cmd](const char* name) { return strcmp(cmd, name) == 0; };
  float a = 0, b = 0, c = 0;
  char extra;

  const bool isMotion = is("WALK") || is("STOP") || is("STAND") || is("SIT") || is("RELAX") || is("POSE");
  if (program && isMotion && joystickBusy(now)) return "ERR busy_joystick";
  const bool limpStays = program && motion.mode() == Mode::Relaxed;   // only STAND, WALK or MOOD wake it

  if (is("PING"))  { walkMs = now; return "OK"; }
  if (is("STAND")) { setPosture(Posture::Stand); return "OK"; }
  if (is("SIT"))   { if (!limpStays) setPosture(Posture::Sit); return "OK"; }
  if (is("RELAX")) { if (!limpStays) setPosture(Posture::Lie); return "OK"; }
  if (is("STOP"))  { if (walking()) setPosture(Posture::Stand); return "OK"; }
  if (is("WALK")) {
    if (sscanf(args, "%f %f %c", &a, &b, &extra) != 2) return "ERR bad_args";
    if (!inRange(a, 80) || !inRange(b, 80)) return "ERR out_of_range";
    startWalk(a, b);               // Motion limits these to MAX_STRIDE and MAX_TURN
    programWalk = program;
    walkMs = now;
    return "OK";
  }
  if (is("POSE")) {
    if (sscanf(args, "%f %f %f %c", &a, &b, &c, &extra) != 3) return "ERR bad_args";
    if (!(a >= 40 && a <= 120) || !inRange(b, 30) || !inRange(c, 30)) return "ERR out_of_range";
    if (limpStays) return "OK";
    heldPose.height = a;
    heldPose.pitch = b;
    heldPose.roll = c;
    setPosture(Posture::Held);
    return "OK";
  }
  if (is("MOOD")) {
    char name[12] = "";
    a = 1;
    const int n = sscanf(args, "%11s %f %c", name, &a, &extra);
    if (n != 2 && !(n == 1 && strchr(args, ' ') == nullptr)) return "ERR bad_args";
    for (int i = 0; i < MOOD_COUNT; i++) {
      if (strcasecmp(name, MOODS[i].name) != 0) continue;
      if (!(a >= 0 && a <= 1)) return "ERR out_of_range";
      setMood(i, a, program, now);
      return "OK";
    }
    return "ERR bad_args";
  }
  if (is("SPEED")) {
    if (sscanf(args, "%f %c", &a, &extra) != 1) return "ERR bad_args";
    if (!(a >= 200 && a <= 2000)) return "ERR out_of_range";
    motion.periodMs = constrain(a, GAIT_PERIOD_MIN_MS, GAIT_PERIOD_MAX_MS);
    return "OK";
  }
  if (is("GAIT")) {
    if (strcmp(args, "crawl") == 0) return "ERR not_implemented";
    return strcmp(args, "trot") == 0 ? "OK" : "ERR bad_args";
  }
  if (is("LOOK") || is("EARS") || is("TAIL") || is("FACE")) return "OK";   // not fitted: ignored
  if (is("STATUS")) {
    Serial.printf("{\"mood\":\"%s\",\"intensity\":%.2f,\"posture\":\"%s\",\"walking\":%s,\"gait\":\"trot\",\"period_ms\":%d,\"uptime_ms\":%lu}\n",
                  MOODS[mood.index].name, mood.intensity, POSTURE_NAME[(int)posture], walking() ? "true" : "false",
                  (int)motion.periodMs, (unsigned long)now);
    return nullptr;
  }
  return "ERR unknown_command";
}

// ---------------- typed commands: tricks and calibration ----------------
void printHelp() {
  Serial.println(F(
    "\nMove     stand  sit  relax  stop  walk <stride> <turn>  (mm, + turn = left; try walk 15 0)\n"
    "         pose <height> <pitch> <roll>   speed <ms>   status\n"
    "Moods    mood <name> [0..1]: idle happy excited sad curious alert scared sleepy angry playful\n"
    "Tricks   bow stretch pushup wiggle dance paw\n"
    "Setup    zero (all servos 90)  limp  test <leg 0-3>  nudge <ch> <deg>  dir <ch> <1|-1>\n"
    "         cal  save  reset"));
}

// Wiggle one leg to check servo directions. Blocks for a few seconds: hold the dog in the air.
void testLeg(int leg) {
  Serial.printf("Testing %s: foot FORWARD, then BACK, then the knee bends MORE.\n", LEG_NAME[leg]);
  const JointAngles j = legIK(0, motion.standHeight);
  const uint8_t hip = HIP_CH[leg], knee = KNEE_CH[leg];
  const int bend = KNEES_BACKWARD ? 1 : -1;
  servos.writeJoint(knee, kneeJoint(j));
  for (int a = 0; a <= 20; a++)   { servos.writeJoint(hip, hipJoint(j) + a); delay(25); }
  for (int a = 20; a >= -20; a--) { servos.writeJoint(hip, hipJoint(j) + a); delay(25); }
  for (int a = -20; a <= 0; a++)  { servos.writeJoint(hip, hipJoint(j) + a); delay(25); }
  for (int a = 0; a <= 20; a++)   { servos.writeJoint(knee, kneeJoint(j) + bend * a); delay(25); }
  for (int a = 20; a >= 0; a--)   { servos.writeJoint(knee, kneeJoint(j) + bend * a); delay(25); }
  Serial.printf("Hip wrong way: dir %d %d.  Knee wrong way: dir %d %d\n", hip, -servos.dir[hip], knee, -servos.dir[knee]);
}

bool runTyped(const char* cmd, const char* args, uint32_t now) {
  const auto is = [cmd](const char* name) { return strcmp(cmd, name) == 0; };
  int n = -1;
  float v = 0;
  const int got = sscanf(args, "%d %f", &n, &v);
  const bool channel = got == 2 && n >= 0 && n < 8 && isfinite(v);

  for (const Trick& t : TRICKS) {
    if (!is(t.name)) continue;
    setPosture(Posture::Trick);
    trick = &t;
    trickKey = 0;
    trickKeyMs = now;
    Serial.printf("Doing %s.\n", t.name);
    return true;
  }
  if (is("help")) {
    printHelp();
  } else if (is("limp")) {
    goLimp();
    servos.relax();
    Serial.println("Limp.");
  } else if (is("zero")) {
    goLimp();
    for (uint8_t ch = 0; ch < 8; ch++) servos.writeDeg(ch, 90);
    Serial.println("All servos at 90.");
  } else if (is("test")) {
    if (got >= 1 && n >= 0 && n < 4) { goLimp(); testLeg(n); }
    else Serial.println("Use: test <leg 0-3>");
  } else if (is("nudge")) {
    if (channel) { servos.offset[n] += v; Serial.printf("ch%d offset %+.1f\n", n, servos.offset[n]); }
    else Serial.println("Use: nudge <ch 0-7> <deg>");
  } else if (is("dir")) {
    if (channel) { servos.dir[n] = v < 0 ? -1 : 1; Serial.printf("ch%d direction %+d\n", n, servos.dir[n]); }
    else Serial.println("Use: dir <ch 0-7> <1|-1>");
  } else if (is("cal")) {
    servos.printCal(Serial);
  } else if (is("save")) {
    servos.saveCal();
    Serial.println("Calibration saved.");
  } else if (is("reset")) {
    servos.resetCal();
    Serial.println("Calibration reset (not saved).");
  } else {
    return false;
  }
  return true;
}

void handleLine(char* line) {
  const uint32_t now = millis();
  const bool program = isupper((unsigned char)line[0]);
  const char* args = "";
  if (char* space = strchr(line, ' ')) { *space = '\0'; args = space + 1; }
  if (!program) {
    if (runTyped(line, args, now)) return;
    for (char* p = line; *p; p++) *p = toupper((unsigned char)*p);
  }
  const char* reply = runCommand(line, args, program, now);
  if (reply) { Serial.print(reply); Serial.print('\n'); }
}

void pollSerial() {
  static char line[64];
  static size_t len = 0;
  static bool tooLong = false;
  while (Serial.available()) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (tooLong) Serial.print("ERR line_too_long\n");
      else if (len) { line[len] = '\0'; handleLine(line); }
      len = 0;
      tooLong = false;
    } else if (len < sizeof line - 1) {
      line[len++] = c;
    } else {
      tooLong = true;
    }
  }
}

// ---------------- joystick: push to walk, press to stand or sit ----------------
int joyCx = 0, joyCy = 0;
bool joyFitted = false;

void beginJoystick() {
  if (!JOYSTICK_ENABLED) return;
  pinMode(PIN_JOY_BTN, INPUT_PULLUP);
  analogReadResolution(12);
  long sx = 0, sy = 0;
  for (int i = 0; i < 32; i++) { sx += analogRead(PIN_JOY_X); sy += analogRead(PIN_JOY_Y); delay(2); }
  joyCx = sx / 32;
  joyCy = sy / 32;
  // A centred stick reads about mid-scale. Anything else means it isn't wired.
  joyFitted = joyCx > 1000 && joyCx < 3100 && joyCy > 1000 && joyCy < 3100;
  Serial.printf("Joystick %s\n", joyFitted ? "ready" : "not found, off");
}

float joyAxis(int raw, int centre) {
  const float v = raw > centre ? (float)(raw - centre) / (4095 - centre) : (float)(raw - centre) / centre;
  return fabsf(v) < 0.12f ? 0.0f : constrain(v, -1.0f, 1.0f);
}

void pollJoystick(uint32_t now) {
  if (!joyFitted) return;
  static bool lastUp = true;
  static uint32_t pressMs = 0;
  const bool up = digitalRead(PIN_JOY_BTN);
  const bool pressed = !up && lastUp && now - pressMs > 250;
  lastUp = up;
  const float x = joyAxis(analogRead(PIN_JOY_X), joyCx), y = -joyAxis(analogRead(PIN_JOY_Y), joyCy);
  if (pressed || x != 0 || y != 0) { joyUsed = true; joyMs = now; }

  if (pressed) {
    pressMs = now;
    const bool standing = motion.mode() == Mode::Pose && posture == Posture::Stand;
    setPosture(standing ? Posture::Sit : Posture::Stand);
  } else if ((x != 0 || y != 0) && motion.mode() != Mode::Relaxed) {
    startWalk(y * MAX_STRIDE, -x * MAX_TURN);
    joyDriving = true;
  } else if (joyDriving) {
    setPosture(Posture::Stand);
  }
}

// ---------------- main ----------------
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== DeskDog basic firmware ===");
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);
  if (!servos.begin()) {
    Serial.println("PCA9685 not found at 0x40. Check SDA=21, SCL=22, 3V3 and GND.");
    while (true) delay(1000);
  }
  beginJoystick();
  Serial.println("Servos are limp. Type 'stand' to start, 'help' for all commands.");
}

void loop() {
  static uint32_t last = millis();
  pollSerial();
  const uint32_t now = millis();
  if (now - last < CONTROL_DT_MS) return;
  const float dt = (now - last) / 1000.0f;
  last = now;
  pollJoystick(now);
  updateSafety(now);
  mood.update(dt);
  applyBody();
  motion.update(dt);
}
