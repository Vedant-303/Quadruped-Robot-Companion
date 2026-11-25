# DeskDog (basic)

A small desk robot dog that walks, does tricks and reacts to your facial expression.

| Folder | What it holds |
|---|---|
| `dog/` | Firmware for the dog's ESP32 (PlatformIO): walking, movements and moods |
| `emotion/` | Python script for your laptop: webcam expression to dog mood |

## Parts and wiring

ESP32 DevKit, PCA9685 servo driver, 8 × MG90S servos and an optional joystick.

| From | To |
|---|---|
| PCA9685 VCC, GND, SDA, SCL | ESP32 3V3, GND, GPIO21, GPIO22 |
| PCA9685 screw terminal (V+, GND) | Servo supply, 5–6 V. Never power the servos from the ESP32 |
| Joystick VRx, VRy, SW | GPIO34, GPIO35, GPIO32 (power it from 3V3) |

| Leg | Hip | Knee |
|---|---|---|
| Front-left | 0 | 1 |
| Front-right | 2 | 3 |
| Rear-left | 4 | 5 |
| Rear-right | 6 | 7 |

All grounds must be joined. Pins and leg sizes are in `dog/src/config.h`.

## 1. The dog

With [PlatformIO](https://platformio.org/) installed:

```
cd dog
pio run -t upload
pio device monitor
```

### First-time setup

Hold the dog in the air for steps 2 and 3.

1. Before fitting the horns, type `zero`. All servos go to 90°, which is the standing pose: thigh tilted about 41° with the knee toward the tail, and the foot under the hip. Fit the horns there.
2. Type `test 0` to `test 3`. Each foot should swing forward, then back, then the knee should bend more. If a joint is wrong, use the `dir` command it prints.
3. Type `stand`. Straighten any joint with, for example, `nudge 2 3`, then type `save`.

### Commands

| Type | What it does |
|---|---|
| `stand`, `sit`, `relax` | Stand, sit, lie down |
| `walk 15 0`, `walk -15 0` | Walk forward, backward (mm per stride) |
| `walk 0 10`, `walk 0 -10` | Turn left, right |
| `stop` | Stop walking |
| `speed 800` | Time of one gait cycle in ms. Slower is steadier |
| `pose 70 5 0` | Hold a height, pitch and roll |
| `bow`, `stretch`, `pushup`, `wiggle`, `dance`, `paw` | Tricks |
| `mood happy`, `mood sad 0.5` | Moods, with an optional strength from 0 to 1 |
| `limp` | Servos off |
| `help` | All commands |

With the joystick, push to walk and turn, and press to stand or sit.

Commands typed in lowercase stay until you change them. Programs send them in UPPERCASE, which adds safety rules: a `WALK` stops unless it is repeated or followed by `PING` within 1 second, a `MOOD` fades after 10 seconds without a repeat, and the joystick always wins. Servos stay limp from power-up until the first `STAND`, `WALK` or `MOOD`.

### Moods

| Mood | What the dog does |
|---|---|
| idle | Breathes slowly |
| happy | Stands taller and bounces |
| excited | Bounces fast and wiggles |
| sad | Low, nose down |
| curious | Tilts its head down to one side |
| alert | Stands tall and freezes |
| scared | Takes two steps back, leans away |
| sleepy | Sits low, breathes slowly |
| angry | Nose down, stiff |
| playful | Play bow with a wiggle |

Change them in the table in `dog/src/moods.h`. Tricks are in `dog/src/tricks.h`.

## 2. Emotions

Needs Python 3.11 or newer.

```
cd emotion
pip install -r requirements.txt
python emotion.py
python emotion.py --port COM3
```

Without `--port` it only shows the moods, so you can try it with no dog. Close the serial monitor first when using a port.

| Your face | Dog's mood |
|---|---|
| Happy | happy |
| Sad | sad |
| Surprised | alert |
| Angry or afraid | scared |
| Disgust or contempt | curious |
| Neutral | idle |
| Nobody there for 5 s, then 60 s | curious, then sleepy |

Everything runs on your computer. No video is saved or sent anywhere. The first run downloads two models (16 MB): [YuNet](https://github.com/opencv/opencv_zoo) for finding faces and [HSEmotion](https://github.com/HSE-asavchenko/face-emotion-recognition) for expressions.
