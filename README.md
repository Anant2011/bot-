# keus_ROBOTICS

> Autonomous line-following robot with real-time object detection, obstacle avoidance, and a live web dashboard.

---

## Project Overview

| Feature | Details |
|---------|---------|
| Line Following | 6-sensor IR array, smooth + sharp + memory-assisted correction |
| Obstacle Avoidance | 3x ultrasonic — dynamic 180 front turn, servo sweep on sides |
| Object Detection | YOLOv8, 18 classes, Raspberry Pi camera |
| Live Dashboard | MJPEG stream + detection panel, Flask port 5000 |
| Run Recording | .avi video + timestamped .txt detection log |

---

## Repository Structure

```
keus_ROBOTICS/
├── Esp_code/
│   └── main.ino          # ESP32: line following + obstacle avoidance
├── ML_train_code/
│   ├── dataset/          # Images and labels (YOLO format)
│   ├── dataset.yaml      # Ultralytics config
│   ├── train_boxes.py    # Train YOLOv8
│   └── test_boxes.py     # Test on webcam
└── Rasberry_pi_code/
    └── main.py           # Flask: stream, inference, recording
```

---

## Hardware

| Component | Details |
|-----------|---------|
| Microcontroller | ESP32-WROOM-32 |
| SBC | Raspberry Pi 4 |
| Camera | Raspberry Pi Camera v3 or HQ |
| Motor Driver | L298N dual H-bridge |
| DC Motors | 4x differential drive |
| IR Sensors | 6-channel digital array |
| Ultrasonic | 3x HC-SR04 (front, left, right) |
| Servo | Standard hobby servo, pin 15 |
| Power | LiPo / battery pack |

---

## ESP32 Firmware — `Esp_code/main.ino`

### Pin Assignments

| Function | Pins |
|----------|------|
| Left motor IN1, IN2, ENA | 22, 23, 18 |
| Right motor IN3, IN4, ENB | 19, 21, 5 |
| IR sensors S0–S5 | 34, 35, 32, 33, 25, 26 |
| Front ultrasonic trig / echo | 27 / 14 |
| Left ultrasonic trig / echo | 12 / 13 |
| Right ultrasonic trig / echo | 2 / 4 |
| Servo | 15 |

---

### Line Following — IR Sensor States

`1` = sees line, `0` = no line

**Forward**

| S0 | S1 | S2 | S3 | S4 | S5 | Action |
|----|----|----|----|----|----|--------|
| 0 | 0 | 1 | 1 | 0 | 0 | Forward |
| 0 | 0 | 1 | 0 | 0 | 0 | Forward |
| 0 | 0 | 0 | 1 | 0 | 0 | Forward |
| 0 | 1 | 1 | 1 | 1 | 0 | Forward |

**Turn Left**

| S0 | S1 | S2 | S3 | S4 | S5 | Action |
|----|----|----|----|----|----|--------|
| 0 | 1 | 1 | 0 | 0 | 0 | Gentle left |
| 0 | 1 | 0 | 0 | 0 | 0 | Gentle left |
| 0 | 1 | 1 | 1 | 0 | 0 | Gentle left |
| 1 | 1 | 0 | 0 | 0 | 0 | Moderate left |
| 1 | 1 | 1 | 0 | 0 | 0 | Moderate left |
| 1 | 0 | 1 | 0 | 0 | 0 | Moderate left |
| 1 | 0 | 0 | 0 | 0 | 0 | Sharp left |
| 1 | 1 | 1 | 1 | 0 | 0 | Sharp left |
| 1 | 1 | 1 | 1 | 1 | 0 | Sharp left |

**Turn Right**

| S0 | S1 | S2 | S3 | S4 | S5 | Action |
|----|----|----|----|----|----|--------|
| 0 | 0 | 0 | 1 | 1 | 0 | Gentle right |
| 0 | 0 | 0 | 0 | 1 | 0 | Gentle right |
| 0 | 0 | 1 | 1 | 1 | 0 | Gentle right |
| 0 | 0 | 0 | 0 | 1 | 1 | Moderate right |
| 0 | 0 | 0 | 1 | 1 | 1 | Moderate right |
| 0 | 0 | 0 | 1 | 0 | 1 | Moderate right |
| 0 | 0 | 0 | 0 | 0 | 1 | Sharp right |
| 0 | 0 | 1 | 1 | 1 | 1 | Sharp right |
| 0 | 1 | 1 | 1 | 1 | 1 | Sharp right |

**Special**

| S0 | S1 | S2 | S3 | S4 | S5 | Action |
|----|----|----|----|----|----|--------|
| 1 | 1 | 1 | 1 | 1 | 1 | Crossroad — brake |
| 0 | 0 | 0 | 0 | 0 | 0 | Line lost — recovery |

---

### Lost-Line Recovery

| Phase | Trigger | Action |
|-------|---------|--------|
| Memory | All sensors zero | Turn in last direction for 100 ms (straight) or 700 ms (turn) |
| Active scan | After memory timeout | Spin until any sensor sees black |
| Failsafe | After 10 s | Hard brake |

---

### Obstacle Avoidance

| Sensor | Threshold | Action |
|--------|-----------|--------|
| Front | < 15 cm | Brake → spin right → re-acquire line → freeze all sensors |
| Left | < 20 cm | Brake → servo 180° → wait 3 s → recenter → freeze 2 s |
| Right | < 20 cm | Brake → servo 0° → wait 3 s → recenter → freeze 2 s |

> Front freeze duration = `2 x (runTime - 3s)`

---

### Speed Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| baseSpeed | 85 | Forward |
| turnSpeed | 135 | Outer wheel, gentle turn |
| innerWheelSpeed | 75 | Inner wheel, gentle turn |
| sharpTurnSpeed | 160 | Outer wheel, sharp turn |
| sharpReverseSpeed | 100 | Inner wheel, sharp turn (reverse) |

---

## ML Training — `ML_train_code/`

### Dataset Structure

```
dataset/
├── train/
│   ├── images/
│   └── labels/    (class cx cy w h)
└── valid/
    ├── images/
    └── labels/
```

### Train

```bash
cd ML_train_code
python train_boxes.py
```

> Output: `runs/detect/trainN/weights/best.pt`
> Use `epochs=50+` for real accuracy — default `epochs=2` is for testing only.

### Test on Webcam

```bash
python test_boxes.py
# Press q to quit
# Raise conf above 0.0032 to reduce false positives
```

---

## Raspberry Pi Server — `Rasberry_pi_code/main.py`

| Component | Detail |
|-----------|--------|
| Camera | Picamera2 640x480, RGB to BGR |
| Inference | Every 3rd frame, imgsz=320, conf=0.5 |
| Recording | MJPG .avi, 10 FPS, 10-min cap, pause/resume |
| Logging | run_timestamp_log.txt, one line per detection |

### API Endpoints

| Route | Description |
|-------|-------------|
| GET / | Web dashboard |
| GET /video_feed | MJPEG stream |
| GET /detections | JSON detected objects |
| GET /start | Start / resume recording |
| GET /stop | Pause recording |

---

## Model Weights

> ⚠️ Not included — configure before running.

1. Download `yolov8n.pt` from [Ultralytics](https://docs.ultralytics.com/models/) → place in `ML_train_code/`
2. Train → copy `best.pt` to Pi → update path in `main.py`

```python
model = YOLO("runs/detect/train/weights/best.pt")
```

---

## Installation

**ESP32**
1. Arduino IDE + ESP32 board support
2. Install `ESP32Servo` via Library Manager
3. Open `main.ino` → select board + port → Upload

**ML Training (PC/GPU)**
```bash
pip install ultralytics opencv-python
```

**Raspberry Pi**
```bash
pip install flask picamera2 ultralytics opencv-python
```

---

## Running

**ESP32** — Power on → 3 s delay → starts automatically. Serial at 9600 baud.

**Raspberry Pi**
```bash
cd Rasberry_pi_code
python main.py
# Open: http://<pi-ip>:5000
```

**Test ML on PC**
```bash
cd ML_train_code
python test_boxes.py
```

---

## Dataset & Classes

| # | Class | Category |
|---|-------|----------|
| 0 | Brandlogo_Maybechlogo | Brand Logo |
| 1 | Brandlogo_Teslalogo | Brand Logo |
| 2 | Brandlogo_applelogo | Brand Logo |
| 3 | Brandlogo_keuslogo | Brand Logo |
| 4 | QRcode | Code |
| 5 | chair | Furniture |
| 6 | faces_RogerFederer | Face |
| 7 | faces_henryCavill | Face |
| 8 | faces_keanureeves | Face |
| 9 | numberplate | Vehicle |
| 10 | parcelbox | Object |
| 11 | pets_cat | Animal |
| 12 | pets_dog | Animal |
| 13 | smartswitch | Electronics |
| 14 | table | Furniture |
| 15 | vehicle_bicycle | Vehicle |
| 16 | vehicle_car | Vehicle |
| 17 | vehicle_motorbike | Vehicle |

---

## Tuning

**ESP32 Speed**
```cpp
const int baseSpeed         = 85;
const int turnSpeed         = 135;
const int innerWheelSpeed   = 75;
const int sharpTurnSpeed    = 160;
const int sharpReverseSpeed = 100;
```

**ESP32 Recovery Timeouts**
```cpp
const unsigned long sweepTimeoutStraight = 100; // ms
const unsigned long sweepTimeoutTurn     = 700; // ms
```

**Pi Inference**

| Setting | Default | Effect |
|---------|---------|--------|
| imgsz | 320 | Lower = faster, 640 = more accurate |
| conf | 0.5 | Raise to cut false positives |
| frame skip | % 3 | Raise for smoother stream |

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Robot drifts | Tune baseSpeed / innerWheelSpeed |
| Ultrasonic reads 999 | Check wiring, ground loops on echo pin |
| 180 turn overshoots | Tune sharpTurnSpeed / sharpReverseSpeed |
| YOLO slow on Pi | imgsz=160, skip every 5th frame, INT8 model |
| picamera2 fails | Enable in raspi-config, test with libcamera-hello |
| best.pt not found | Use absolute path: YOLO("/home/pi/weights/best.pt") |
| Stream blank | Flask on 0.0.0.0, allow port 5000 in firewall |
| Side sensor false trigger | Check freezeDuration constant |
