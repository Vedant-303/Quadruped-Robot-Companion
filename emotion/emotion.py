"""Webcam -> your facial expression -> the dog's mood.

    python emotion.py                 show the moods only, no dog needed
    python emotion.py --port COM3     send them to the dog over USB

Everything runs on this computer: no video is saved or sent anywhere. The first run downloads two
small models (16 MB). Press q in the window, or Ctrl+C, to stop.
"""

import argparse
import hashlib
import math
import time
import urllib.request
from pathlib import Path

import cv2
import numpy as np
import onnxruntime as ort

MODELS = Path(__file__).resolve().parent / "models"
FACE_MODEL = (
    "face_detection_yunet_2023mar.onnx",
    "https://github.com/opencv/opencv_zoo/raw/main/models/face_detection_yunet/"
    "face_detection_yunet_2023mar.onnx",
    "8f2383e4dd3cfbb4553ea8718107fc0423210dc964f9f4280604804ed2552fa4",
)
EMOTION_MODEL = (
    "enet_b0_8_va_mtl.onnx",
    "https://github.com/HSE-asavchenko/face-emotion-recognition/raw/main/models/"
    "affectnet_emotions/onnx/enet_b0_8_va_mtl.onnx",
    "c43e056ad388d4a8dc911832b8291435b2af537f967e5870ebd731574ec7e812",
)

# The model's eight outputs, in order, and the mood the dog shows for each.
LABELS = ("anger", "contempt", "disgust", "fear", "happiness", "neutral", "sadness", "surprise")
MOOD_FOR = {
    "anger": "scared",
    "contempt": "curious",
    "disgust": "curious",
    "fear": "scared",
    "happiness": "happy",
    "neutral": "idle",
    "sadness": "sad",
    "surprise": "alert",
}

SMOOTHING_S = 0.8  # averages the expression over about this long
MIN_SURE = 0.45  # ignore expressions weaker than this
DWELL_S = 1.0  # a new mood must last this long before the dog changes
CURIOUS_AFTER_S = 5.0  # nobody in view: look around...
SLEEPY_AFTER_S = 60.0  # ...then doze off
KEEPALIVE_S = 2.0  # the dog goes back to idle if it hears nothing for 10 s

MEAN = np.array([0.485, 0.456, 0.406], dtype=np.float32)
STD = np.array([0.229, 0.224, 0.225], dtype=np.float32)


def get_model(name: str, url: str, sha256: str) -> str:
    path = MODELS / name
    if not path.exists():
        print(f"downloading {name}...")
        MODELS.mkdir(exist_ok=True)
        with urllib.request.urlopen(url, timeout=60) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != sha256:
            raise SystemExit(f"{name}: the download doesn't match its checksum")
        path.write_bytes(data)
    return str(path)


class FaceReader:
    """Finds the largest face and reads its expression as eight probabilities."""

    def __init__(self) -> None:
        self.detector = cv2.FaceDetectorYN.create(get_model(*FACE_MODEL), "", (320, 320), 0.7)
        self.session = ort.InferenceSession(
            get_model(*EMOTION_MODEL), providers=["CPUExecutionProvider"]
        )
        self.input = self.session.get_inputs()[0].name

    def read(self, frame: np.ndarray):
        """Returns (probabilities, (x0, y0, x1, y1)), or None if there's no face."""
        height, width = frame.shape[:2]
        scale = min(1.0, 320 / width)
        small = cv2.resize(frame, (round(width * scale), round(height * scale)))
        self.detector.setInputSize((small.shape[1], small.shape[0]))
        _, faces = self.detector.detect(small)
        if faces is None:
            return None
        x, y, w, h = max(faces, key=lambda f: f[2] * f[3])[:4] / scale
        x0, y0 = max(0, round(x)), max(0, round(y))
        x1, y1 = min(width, round(x + w)), min(height, round(y + h))
        if x1 - x0 < 40 or y1 - y0 < 40:
            return None
        face = cv2.cvtColor(frame[y0:y1, x0:x1], cv2.COLOR_BGR2RGB)
        face = (cv2.resize(face, (224, 224)).astype(np.float32) / 255.0 - MEAN) / STD
        out = self.session.run(None, {self.input: face.transpose(2, 0, 1)[None]})[0][0][:8]
        e = np.exp(out - out.max())
        return e / e.sum(), (x0, y0, x1, y1)


class MoodPicker:
    """Turns expression readings into one steady mood and intensity."""

    def __init__(self, now: float) -> None:
        self.mood, self.intensity = "idle", 0.5
        self.probs: np.ndarray | None = None
        self.last_time = self.last_face = now
        self.candidate, self.candidate_since = "idle", now

    def update(self, probs: np.ndarray | None, now: float) -> None:
        dt, self.last_time = now - self.last_time, now
        if probs is not None:
            self.last_face = now
            k = 1 - math.exp(-dt / SMOOTHING_S)
            self.probs = probs if self.probs is None else self.probs + k * (probs - self.probs)
        wanted, intensity = self._wanted(now)
        if wanted != self.candidate:
            self.candidate, self.candidate_since = wanted, now
        if wanted == self.mood:
            self.intensity = intensity
        elif now - self.candidate_since >= DWELL_S:
            self.mood, self.intensity = wanted, intensity

    def _wanted(self, now: float) -> tuple[str, float]:
        away = now - self.last_face
        if away >= SLEEPY_AFTER_S:
            return "sleepy", 1.0
        if away >= CURIOUS_AFTER_S:
            self.probs = None
            return "curious", 0.6
        if self.probs is None:
            return self.mood, self.intensity
        best = int(self.probs.argmax())
        sure = float(self.probs[best])
        if sure < MIN_SURE:
            return self.mood, self.intensity
        return MOOD_FOR[LABELS[best]], min(1.0, 0.4 + 0.6 * sure)


class Dog:
    """The dog on a USB serial port. With no port, it only prints."""

    def __init__(self, port: str | None) -> None:
        self.serial = None
        self.last = ("", 0.0)
        self.last_sent = 0.0
        if port:
            import serial

            self.serial = serial.serial_for_url(port, 115200, timeout=0, write_timeout=1)
            time.sleep(2)  # opening the port restarts the ESP32
            self.serial.reset_input_buffer()

    def show(self, mood: str, intensity: float, now: float) -> None:
        changed = mood != self.last[0]
        if not changed and now - self.last_sent < KEEPALIVE_S:
            return
        self.last, self.last_sent = (mood, intensity), now
        if changed:
            print(f"MOOD {mood} {intensity:.2f}", flush=True)
        if self.serial:
            self.serial.write(f"MOOD {mood} {intensity:.2f}\n".encode("ascii"))
            replies = self.serial.read(self.serial.in_waiting or 1).decode(errors="replace")
            for line in replies.splitlines():
                if line.startswith("ERR"):
                    print("dog:", line)

    def close(self) -> None:
        if self.serial:
            self.serial.write(b"MOOD idle 0\n")
            self.serial.close()


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--port", help="the dog's serial port, such as COM3")
    parser.add_argument("--camera", type=int, default=0)
    parser.add_argument("--no-window", action="store_true")
    args = parser.parse_args()

    reader = FaceReader()
    camera = cv2.VideoCapture(args.camera)
    if not camera.isOpened():
        raise SystemExit(f"cannot open camera {args.camera}")
    dog = Dog(args.port)
    picker = MoodPicker(time.monotonic())
    try:
        while True:
            ok, frame = camera.read()
            if not ok:
                raise SystemExit("the camera stopped")
            now = time.monotonic()
            result = reader.read(frame)
            picker.update(result[0] if result else None, now)
            dog.show(picker.mood, picker.intensity, now)
            if args.no_window:
                continue
            if result:
                x0, y0, x1, y1 = result[1]
                cv2.rectangle(frame, (x0, y0), (x1, y1), (0, 200, 0), 2)
                label = LABELS[int(result[0].argmax())]
                cv2.putText(frame, label, (x0, max(20, y0 - 8)), 0, 0.7, (0, 200, 0), 2)
            text = f"dog: {picker.mood} {picker.intensity:.2f}"
            cv2.putText(frame, text, (10, frame.shape[0] - 12), 0, 0.8, (255, 255, 255), 2)
            cv2.imshow("DeskDog", frame)
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
    except KeyboardInterrupt:
        pass
    finally:
        camera.release()
        cv2.destroyAllWindows()
        dog.close()


if __name__ == "__main__":
    main()
