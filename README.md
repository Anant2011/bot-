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
`keus_ROBOTICS` combines embedded control, computer vision, and a web interface in one system.
Feature	Details
Line Following	6-sensor IR array — smooth, sharp, and memory-assisted correction
Obstacle Avoidance	3× ultrasonic sensors — dynamic 180° front turn, servo sweep on sides
Object Detection	Custom YOLOv8 model, 18 classes, runs on Raspberry Pi camera
Live Dashboard	MJPEG stream + detection panel via Flask on port 5000
Run Recording	`.avi` video + timestamped `.txt` detection log per session
---
Repository Structure
```
keus_ROBOTICS/
├── Esp_code/
│   └── main.ino              # ESP32: line following + obstacle avoidance
│
├── ML_train_code/
│   ├── dataset/              # Training images and labels (YOLO format)
│   ├── dataset.yaml          # Ultralytics dataset config
│   ├── train_boxes.py        # Train the YOLOv8 model
│   └── test_boxes.py         # Test model on webcam
│
└── Rasberry_pi_code/
    └── main.py               # Flask server: stream, inference, recording
```
---
Hardware Requirements
Component	Details
Microcontroller	ESP32-WROOM-32
Single-Board Computer	Raspberry Pi 4 (any Pi with `picamera2` support)
Camera	Raspberry Pi Camera Module v3 or HQ
Motor Driver	L298N dual H-bridge
DC Motors	4× (differential drive)
IR Sensor Array	6-channel digital
Ultrasonic Sensors	3× HC-SR04 (front, left, right)
Servo	Standard hobby servo on pin 15
Power	LiPo / battery pack for motors + logic
---
System Architecture
```
┌─────────────────────────────────┐
│          Raspberry Pi           │
│  ┌────────────────────────────┐ │
│  │  main.py (Flask server)    │ │
│  │  - PiCamera2 capture       │ │
│  │  - YOLOv8 inference        │ │
│  │  - MJPEG stream            │ │
│  │  - /start /stop recording  │ │
│  └──────────────┬─────────────┘ │
│                 │ HTTP :5000     │
└─────────────────┼───────────────┘
                  │ Wi-Fi
        ┌─────────▼──────────┐
        │   Web Browser      │
        │   Dashboard        │
        └────────────────────┘

┌─────────────────────────────────┐
│              ESP32              │
│  ┌────────────────────────────┐ │
│  │  main.ino                  │ │
│  │  - 6× IR sensor array      │ │
│  │  - 3× Ultrasonic sensors   │ │
│  │  - H-bridge motor control  │ │
│  │  - Servo head              │ │
│  └────────────────────────────┘ │
└─────────────────────────────────┘
```
> The ESP32 and Raspberry Pi run independently on the same chassis.  
> ESP32 → real-time control. Raspberry Pi → vision + dashboard.
---
Module Documentation
ESP32 Firmware — `Esp_code/main.ino`
Pin Assignments
Function	Pins
Left motor (IN1, IN2, ENA)	22, 23, 18
Right motor (IN3, IN4, ENB)	19, 21, 5
IR sensors S[0]–S[5]	34, 35, 32, 33, 25, 26
Front ultrasonic (trig / echo)	27 / 14
Left ultrasonic (trig / echo)	12 / 13
Right ultrasonic (trig / echo)	2 / 4
Servo (head)	15
Line Following Logic
6 IR sensors produce a 6-bit state, mapped to a drive behavior:
Sensor Pattern	Behavior
`001100` `001000` `000100` `011110`	Forward
`011000` `010000` `011100`	Gentle left — inner wheel slowed
`110000` `111000` `101000`	Moderate left — inner wheel stopped
`100000` `111100` `111110`	Sharp left — inner wheel reversed briefly
`000110` `000010` `001110`	Gentle right — inner wheel slowed
`000011` `000111` `000101`	Moderate right — inner wheel stopped
`000001` `001111` `011111`	Sharp right — inner wheel reversed briefly
`111111`	All black / crossroad — brake
`000000`	Line lost — enter recovery
Lost-Line Recovery
Phase	Action
Memory	Continue in `lastDirection` for `sweepTimeoutStraight` (100 ms) or `sweepTimeoutTurn` (700 ms)
Active scan	Keep spinning in last known direction until any sensor sees black; hard brake after 10 s
Obstacle Avoidance
Sensor	Threshold	Response
Front	< 15 cm	`execute180Turn()` — brake → spin right → wait for center sensors to re-acquire line → freeze all sensors for `2 × (runTime − 3 s)`
Left	< 20 cm	Brake → servo to 180° → wait 3 s → recenter → freeze left sensor 2 s
Right	< 20 cm	Brake → servo to 0° → wait 3 s → recenter → freeze right sensor 2 s
Speed Constants
Constant	Value	Purpose
`baseSpeed`	85	Forward speed
`turnSpeed`	135	Outer wheel — gentle turn
`innerWheelSpeed`	75	Inner wheel — gentle turn
`sharpTurnSpeed`	160	Outer wheel — sharp turn
`sharpReverseSpeed`	100	Inner wheel — sharp turn (reverse)
---
ML Training — `ML_train_code/`
Dataset Structure
```
dataset/
├── train/
│   ├── images/        # .jpg / .png
│   └── labels/        # YOLO format .txt  (class cx cy w h)
└── valid/
    ├── images/
    └── labels/
```
`train_boxes.py` — Train
```python
from ultralytics import YOLO
model = YOLO("yolov8n.pt")
model.train(data="dataset.yaml", epochs=2, imgsz=640)
```
```bash
python train_boxes.py
```
> Weights saved to `runs/detect/trainN/weights/best.pt`  
> Use `epochs=50+` for real accuracy — `2` is a smoke-test value only.
`test_boxes.py` — Test on Webcam
```python
model = YOLO("runs/detect/train/weights/best.pt")
cap = cv2.VideoCapture(0)   # change index for external camera
```
```bash
python test_boxes.py
```
> Press `q` to quit.  
> Default `conf=0.0032` — raise it to reduce false positives.
---
Raspberry Pi Server — `Rasberry_pi_code/main.py`
How It Works
Component	Detail
Camera	`Picamera2` at 640×480, RGB → BGR conversion
Inference	Every 3rd frame at `imgsz=320`, `conf=0.5`
Recording	MJPG `.avi` at 10 FPS, 10-minute cap, pause/resume supported
Logging	`run_<timestamp>_log.txt` — one `HH:MM:SS : label` line per detection
API Endpoints
Route	Method	Description
`/`	GET	Web dashboard
`/video_feed`	GET	MJPEG camera stream
`/detections`	GET	JSON — currently detected objects

`/start`	GET	Start / resume recording
`/stop`	GET	Pause recording
---
Model Weights Configuration
> ⚠️ Weights are **not included** in this repo. Configure them before running.
Step 1 — Base weights (for training)
Download `yolov8n.pt` from Ultralytics and place it in `ML_train_code/`.
```python
# train_boxes.py
model = YOLO("yolov8n.pt")
```
Step 2 — Trained weights (for inference)
After training, copy `best.pt` to the Raspberry Pi and update the path.
```python
# test_boxes.py
model = YOLO("runs/detect/train/weights/best.pt")

# main.py
model = YOLO("runs/detect/train/weights/best.pt")
```
Recommended Workflow
```
Train on PC / GPU  →  copy best.pt to Pi  →  update path in main.py
```
---
Setup & Installation
ESP32
Install Arduino IDE + ESP32 board support
Install `ESP32Servo` via Library Manager
Open `Esp_code/main.ino`
Select board + COM port → Upload
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
> Update `YOLO(...)` path in `main.py` to your `best.pt` location.
---
Running the Project
ESP32
Power on → waits 3 s → begins line following automatically
Serial debug output at `9600` baud
Raspberry Pi Server
```bash
cd Rasberry_pi_code
python main.py
```
Open in any browser on the same network:
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
Live video feed	Camera stream with YOLO bounding boxes overlaid
Detected Objects	Updates every 1 s via `/detections` polling
Start Recording	Saves `.avi` video + `.txt` detection log
Pause Recording	Pauses session; elapsed time is preserved for resume
On-screen OSD shows:
Elapsed recorded time
Remaining time (10-minute cap)
Status: `Recording` / `Paused`
---
Dataset & Classes
18 detection classes across 6 categories:
Index	Class	Category
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
> Label format: YOLO `.txt` — `class cx cy w h` (normalized, one object per line)
---
Tuning & Configuration
ESP32 — Speed
```cpp
const int baseSpeed          = 85;   // Forward speed (0–255)
const int turnSpeed          = 135;  // Outer wheel, gentle turn
const int innerWheelSpeed    = 75;   // Inner wheel, gentle turn
const int sharpTurnSpeed     = 160;  // Outer wheel, sharp turn
const int sharpReverseSpeed  = 100;  // Inner wheel, sharp turn (reverse)
```
ESP32 — Lost-Line Timeouts
```cpp
const unsigned long sweepTimeoutStraight = 100;  // ms — robot was going straight
const unsigned long sweepTimeoutTurn     = 700;  // ms — robot was mid-turn
```
Raspberry Pi — Inference
```python
results = model(frame, imgsz=320, conf=0.5)
```
Parameter	Effect
`imgsz=320`	Lower = faster; raise to `640` for more accuracy
`conf=0.5`	Minimum confidence; raise to reduce false positives
`% 3`	Inference every Nth frame; raise for smoother stream
Raspberry Pi — Recording
```python
max_record_time = 600   # seconds (default 10 min)
fps = 10                # output video FPS
```
---
Troubleshooting
Problem	Fix
Robot drifts on straight line	Adjust IR sensor height; tune `baseSpeed` / `innerWheelSpeed`
Ultrasonic reads `999` constantly	Check wiring; look for ground loops on trigger/echo pins
180° turn doesn't complete	Lower sensor array; tune `sharpTurnSpeed` / `sharpReverseSpeed`
YOLO too slow on Pi	Set `imgsz=160`, increase frame-skip to `% 5`, use INT8 model
`picamera2` fails to start	Enable camera in `raspi-config`; run `libcamera-hello` to verify
`best.pt` not found	Use absolute path: `model = YOLO("/home/pi/weights/best.pt")`
Video stream blank in browser	Confirm Flask binds `0.0.0.0`; allow port 5000 in firewall
Side sensors false-trigger in turns	Already suppressed by dynamic freeze logic; check `freezeDuration` if it persists
