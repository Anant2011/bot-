keus_ROBOTICS
> Autonomous line-following robot with real-time object detection, obstacle avoidance, and a live web dashboard.
---
Table of Contents
Project Overview
Repository Structure
Hardware Requirements
System Architecture
Module Documentation
ESP32 Firmware
ML Training
Raspberry Pi Server
Model Weights Configuration
Setup & Installation
Running the Project
Dashboard Usage
Dataset & Classes
Tuning & Configuration
Troubleshooting
---
Project Overview
Feature	Details
Line Following	6-sensor IR array, smooth + sharp + memory-assisted correction
Obstacle Avoidance	3× ultrasonic — dynamic 180° front turn, servo sweep on sides
Object Detection	YOLOv8, 18 classes, runs on Raspberry Pi camera
Live Dashboard	MJPEG stream + detection panel, Flask on port 5000
Run Recording	`.avi` video + timestamped `.txt` detection log
---
Repository Structure
```
keus_ROBOTICS/
├── Esp_code/
│   └── main.ino          # ESP32: line following + obstacle avoidance
├── ML_train_code/
│   ├── dataset/          # Training images and labels (YOLO format)
│   ├── dataset.yaml      # Ultralytics dataset config
│   ├── train_boxes.py    # Train the YOLOv8 model
│   └── test_boxes.py     # Test model on webcam
└── Rasberry_pi_code/
    └── main.py           # Flask server: stream, inference, recording
```
---
Hardware Requirements
Component	Details
Microcontroller	ESP32-WROOM-32
Single-Board Computer	Raspberry Pi 4
Camera	Raspberry Pi Camera v3 or HQ
Motor Driver	L298N dual H-bridge
DC Motors	4× differential drive
IR Sensor Array	6-channel digital
Ultrasonic Sensors	3× HC-SR04 (front, left, right)
Servo	Standard hobby servo, pin 15
Power	LiPo / battery pack
---
System Architecture
```
┌──────────────────────────┐
│       Raspberry Pi       │
│  main.py (Flask :5000)   │
│  - PiCamera2 capture     │
│  - YOLOv8 inference      │
│  - MJPEG stream          │
│  - start/stop recording  │
└────────────┬─────────────┘
             │ Wi-Fi / Browser
┌────────────▼─────────────┐
│       Web Dashboard      │
└──────────────────────────┘

┌──────────────────────────┐
│          ESP32           │
│  main.ino                │
│  - 6× IR sensors         │
│  - 3× Ultrasonic         │
│  - H-bridge motors       │
│  - Servo head            │
└──────────────────────────┘
```
> ESP32 = real-time control. Raspberry Pi = vision + dashboard. Both run independently.
---
Module Documentation
ESP32 Firmware — `Esp_code/main.ino`
Pin Assignments
Function	Pins
Left motor IN1, IN2, ENA	22, 23, 18
Right motor IN3, IN4, ENB	19, 21, 5
IR sensors S[0]–S[5]	34, 35, 32, 33, 25, 26
Front ultrasonic trig/echo	27 / 14
Left ultrasonic trig/echo	12 / 13
Right ultrasonic trig/echo	2 / 4
Servo	15
---
Line Following
Forward patterns
Pattern	Action
`001100` `001000` `000100` `011110`	Drive forward
Left corrections
Pattern	Action
`011000` `010000` `011100`	Gentle left
`110000` `111000` `101000`	Moderate left
`100000` `111100` `111110`	Sharp left
Right corrections
Pattern	Action
`000110` `000010` `001110`	Gentle right
`000011` `000111` `000101`	Moderate right
`000001` `001111` `011111`	Sharp right
Special states
Pattern	Action
`111111`	All black / crossroad — brake
`000000`	Line lost — enter recovery
---
Lost-Line Recovery
Phase	Trigger	Action
Memory	Immediately on `000000`	Keep turning in `lastDirection` for 100 ms (straight) or 700 ms (turn)
Active scan	After memory timeout	Keep spinning until any sensor sees black
Failsafe	After 10 s in active scan	Hard brake
---
Obstacle Avoidance
Sensor	Threshold	Action
Front	< 15 cm	Brake → spin right → wait for center sensors to see line → freeze all sensors
Left	< 20 cm	Brake → servo 180° → wait 3 s → recenter → freeze left 2 s
Right	< 20 cm	Brake → servo 0° → wait 3 s → recenter → freeze right 2 s
> Front freeze duration is dynamic: `2 × (runTime − 3 s)`
---
Speed Constants
Constant	Value	Purpose
`baseSpeed`	85	Forward
`turnSpeed`	135	Outer wheel, gentle turn
`innerWheelSpeed`	75	Inner wheel, gentle turn
`sharpTurnSpeed`	160	Outer wheel, sharp turn
`sharpReverseSpeed`	100	Inner wheel reversed, sharp turn
---
ML Training — `ML_train_code/`
Dataset Folder Structure
```
dataset/
├── train/
│   ├── images/     # .jpg / .png
│   └── labels/     # .txt  (class cx cy w h)
└── valid/
    ├── images/
    └── labels/
```
Train — `train_boxes.py`
```python
from ultralytics import YOLO
model = YOLO("yolov8n.pt")
model.train(data="dataset.yaml", epochs=2, imgsz=640)
```
```bash
python train_boxes.py
```
> Output: `runs/detect/trainN/weights/best.pt`
> Use `epochs=50+` for real accuracy. `epochs=2` is for quick testing only.
Test on Webcam — `test_boxes.py`
```python
model = YOLO("runs/detect/train/weights/best.pt")
cap = cv2.VideoCapture(0)
```
```bash
python test_boxes.py
```
> Press `q` to quit. Default `conf=0.0032` — raise to reduce false positives.
---
Raspberry Pi Server — `Rasberry_pi_code/main.py`
Component	Detail
Camera	Picamera2 at 640×480, RGB→BGR
Inference	Every 3rd frame, `imgsz=320`, `conf=0.5`
Recording	MJPG `.avi`, 10 FPS, 10-min cap, pause/resume
Logging	`run_<timestamp>_log.txt`, one line per detection
API Endpoints
Route	Description
`GET /`	Web dashboard
`GET /video_feed`	MJPEG stream
`GET /detections`	JSON — detected objects
`GET /start`	Start / resume recording
`GET /stop`	Pause recording
---
Model Weights Configuration
> ⚠️ Weights are **not included**. Configure before running.
Step 1 — Base weights (training only)
Download `yolov8n.pt` from Ultralytics → place in `ML_train_code/`.
Step 2 — Trained weights (inference)
```python
# test_boxes.py  and  main.py
model = YOLO("runs/detect/train/weights/best.pt")
```
Workflow
```
Train on PC/GPU  →  copy best.pt to Pi  →  update path in main.py
```
---
Setup & Installation
ESP32
Install Arduino IDE + ESP32 board support
Install `ESP32Servo` via Library Manager
Open `Esp_code/main.ino` → select board + port → Upload
ML Training (PC / GPU)
```bash
pip install ultralytics opencv-python
cd ML_train_code
python train_boxes.py
```
Raspberry Pi
```bash
pip install flask picamera2 ultralytics opencv-python
```
---
Running the Project
ESP32
```
Power on → 3 s delay → line following starts automatically
Serial debug: 9600 baud
```
Raspberry Pi
```bash
cd Rasberry_pi_code
python main.py
```
```
http://<raspberry-pi-ip>:5000
```
Test ML on PC (no Pi needed)
```bash
cd ML_train_code
python test_boxes.py
```
---
Dashboard Usage
Element	Function
Live feed	Camera stream with bounding boxes
Detected Objects	Refreshes every 1 s
Start Recording	Saves `.avi` + `.txt` log
Pause Recording	Pauses; elapsed time is kept
OSD overlays: elapsed time · remaining time · Recording / Paused status
---
Dataset & Classes
#	Class	Category
0	`Brandlogo_Maybechlogo`	Brand Logo
1	`Brandlogo_Teslalogo`	Brand Logo
2	`Brandlogo_applelogo`	Brand Logo
3	`Brandlogo_keuslogo`	Brand Logo
4	`QRcode`	Code
5	`chair`	Furniture
6	`faces_RogerFederer`	Face
7	`faces_henryCavill`	Face
8	`faces_keanureeves`	Face
9	`numberplate`	Vehicle
10	`parcelbox`	Object
11	`pets_cat`	Animal
12	`pets_dog`	Animal
13	`smartswitch`	Electronics
14	`table`	Furniture
15	`vehicle_bicycle`	Vehicle
16	`vehicle_car`	Vehicle
17	`vehicle_motorbike`	Vehicle
---
Tuning & Configuration
ESP32 — Speed
```cpp
const int baseSpeed         = 85;   // forward (0–255)
const int turnSpeed         = 135;  // outer wheel, gentle turn
const int innerWheelSpeed   = 75;   // inner wheel, gentle turn
const int sharpTurnSpeed    = 160;  // outer wheel, sharp turn
const int sharpReverseSpeed = 100;  // inner wheel reversed, sharp turn
```
ESP32 — Recovery Timeouts
```cpp
const unsigned long sweepTimeoutStraight = 100;  // ms
const unsigned long sweepTimeoutTurn     = 700;  // ms
```
Raspberry Pi — Inference
Setting	Default	Effect
`imgsz`	320	Lower = faster; 640 = more accurate
`conf`	0.5	Raise to cut false positives
`% 3`	every 3rd frame	Raise for smoother stream
Raspberry Pi — Recording
```python
max_record_time = 600   # seconds
fps = 10
```
---
Troubleshooting
Problem	Fix
Robot drifts straight	Adjust sensor height; tune `baseSpeed` / `innerWheelSpeed`
Ultrasonic reads 999	Check wiring, look for ground loops on echo pin
180° turn overshoots	Lower sensor array; tune `sharpTurnSpeed` / `sharpReverseSpeed`
YOLO too slow on Pi	Use `imgsz=160`, frame-skip `% 5`, INT8 model
`picamera2` fails	Enable camera in `raspi-config`; test with `libcamera-hello`
`best.pt` not found	Use full path: `YOLO("/home/pi/weights/best.pt")`
Stream blank in browser	Flask must bind `0.0.0.0`; allow port 5000 in firewall
Side sensors false-trigger	Dynamic freeze logic handles this; check `freezeDuration` if it persists
