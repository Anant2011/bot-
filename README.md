
An autonomous line-following robot with real-time object detection, obstacle avoidance, and a Raspberry Pi web dashboard.
---
Table of Contents
Project Overview
Repository Structure
Hardware Requirements
System Architecture
Module Documentation
ESP32 Firmware — `Esp_code/main.ino`
ML Training — `ML_train_code/`
Raspberry Pi Server — `Rasberry_pi_code/main.py`
Model Weights Configuration
Setup & Installation
Running the Project
Dashboard Usage
Dataset & Classes
Tuning & Configuration
Troubleshooting
---
Project Overview
`keus_ROBOTICS` is a full-stack robotics project combining embedded real-time control, computer vision, and a web interface into a single cohesive system. The robot:
Follows a line autonomously using a 6-sensor IR array with smooth, sharp, and memory-assisted turn correction
Avoids obstacles using three ultrasonic sensors (front, left, right), executing a dynamic 180° turn on front detection and a servo-sweep inspection on side detection
Detects 18 object classes in real time using a custom-trained YOLOv8 model running on a Raspberry Pi camera — including brand logos, faces, vehicles, pets, QR codes, and more
Streams video and detection data live to a web dashboard accessible from any device on the local network
Records runs to `.avi` video files with timestamped detection logs
---
Repository Structure
```
keus_ROBOTICS/
├── Esp_code/
│   └── main.ino              # ESP32 firmware: line following + obstacle avoidance
│
├── ML_train_code/
│   ├── dataset/              # Training images and labels (YOLO format)
│   ├── dataset.yaml          # Dataset configuration for Ultralytics YOLO
│   ├── train_boxes.py        # Script to train the YOLOv8 detection model
│   └── test_boxes.py         # Script to test trained model on a webcam
│
└── Rasberry_pi_code/
    └── main.py               # Flask server: camera stream, YOLO inference, recording
```
---
Hardware Requirements
Component	Details
Microcontroller	ESP32 (tested with ESP32-WROOM-32)
Single-Board Computer	Raspberry Pi (4 recommended; any Pi with `picamera2` support)
Camera	Raspberry Pi Camera Module (v3 or HQ)
Motor Driver	L298N or compatible dual H-bridge
DC Motors	4× (differential drive)
IR Sensor Array	6-channel digital IR line sensor
Ultrasonic Sensors	3× HC-SR04 (front, left, right)
Servo Motor	Standard hobby servo (connected to pin 15 on ESP32)
Power Supply	Suitable LiPo or battery pack for motors + logic
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
The ESP32 and Raspberry Pi operate independently on the same robot chassis. The ESP32 handles all low-level control in real time. The Raspberry Pi handles vision and user-facing features.
---
Module Documentation
ESP32 Firmware — `Esp_code/main.ino`
This firmware runs on the ESP32 and handles all real-time motor and sensor control.
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
The 6 IR sensors produce a binary 6-bit state. The firmware maps each state to a driving behavior:
Sensor Pattern	Behavior
`001100`, `001000`, `000100`, `011110`	Drive forward
`011000`, `010000`, `011100`	Gentle left (inner wheel slowed)
`110000`, `111000`, `101000`	Moderate left (inner wheel stopped)
`100000`, `111100`, `111110`	Sharp left (inner wheel reversed briefly)
`000110`, `000010`, `001110`	Gentle right (inner wheel slowed)
`000011`, `000111`, `000101`	Moderate right (inner wheel stopped)
`000001`, `001111`, `011111`	Sharp right (inner wheel reversed briefly)
`111111`	All black (crossroad) — brake
`000000`	Line lost — memory sweep then active scan
Lost-Line Recovery
When all sensors read `0`, the firmware enters a two-phase recovery:
Memory phase — continues turning in `lastDirection` for a short timeout (`sweepTimeoutStraight` = 100 ms for straight, `sweepTimeoutTurn` = 700 ms for turns)
Active scan phase — keeps spinning in the last known direction until any sensor detects black again, with a 10-second hard brake failsafe
Obstacle Avoidance
Front (`< 15 cm`): Executes `execute180Turn()` — brakes, spins right until center sensors re-acquire the line behind, then mutes all ultrasonic sensors for a dynamically calculated freeze duration based on how long the robot had been running: `freeze = 2 × (runTime − 3000 ms)`
Left (`< 20 cm`): Brakes, sweeps servo to 180° for 3 seconds, recenters, then freezes the left sensor for 2 seconds
Right (`< 20 cm`): Brakes, sweeps servo to 0° for 3 seconds, recenters, then freezes the right sensor for 2 seconds
Speed Constants
Constant	Default Value	Purpose
`baseSpeed`	85	Normal forward speed
`turnSpeed`	135	Outer wheel speed during gentle turns
`innerWheelSpeed`	75	Inner wheel speed during gentle turns
`sharpTurnSpeed`	160	Outer wheel speed during sharp turns
`sharpReverseSpeed`	100	Inner wheel reverse speed during sharp turns
---
ML Training — `ML_train_code/`
This module contains all code and configuration needed to train and test the object detection model.
`dataset.yaml`
Defines the dataset structure for Ultralytics YOLO. The model is trained on 18 classes:
```yaml
path: dataset
train: train/images
val: valid/images

nc: 18
names:
  - Brandlogo_Maybechlogo
  - Brandlogo_Teslalogo
  - Brandlogo_applelogo
  - Brandlogo_keuslogo
  - QRcode
  - chair
  - faces_RogerFederer
  - faces_henryCavill
  - faces_keanureeves
  - numberplate
  - parcelbox
  - pets_cat
  - pets_dog
  - smartswitch
  - table
  - vehicle_bicycle
  - vehicle_car
  - vehicle_motorbike
```
Ensure your dataset folder is structured as:
```
dataset/
├── train/
│   ├── images/   # .jpg / .png files
│   └── labels/   # .txt files (YOLO format bounding boxes)
└── valid/
    ├── images/
    └── labels/
```
`train_boxes.py` — Training
```python
from ultralytics import YOLO

model = YOLO("yolov8n.pt")
model.train(
    data="dataset.yaml",
    epochs=2,
    imgsz=640
)
```
Run with:
```bash
python train_boxes.py
```
Trained weights are saved to `runs/detect/trainN/weights/best.pt`. Increase `epochs` significantly (50–100+) for production-grade accuracy — 2 epochs is for quick smoke-testing only.
`test_boxes.py` — Live Webcam Testing
```python
from ultralytics import YOLO
import cv2

model = YOLO("runs/detect/train/weights/best.pt")
cap = cv2.VideoCapture(0)   # change index for external camera
```
Run with:
```bash
python test_boxes.py
```
Press `q` to quit. The confidence threshold is set to `0.0032` by default — raise it to reduce false positives in a real environment.
---
Raspberry Pi Server — `Rasberry_pi_code/main.py`
A Flask web server that captures frames from the Pi camera, runs YOLO inference, streams video to a browser, and handles recording.
Key Components
Camera: Uses `Picamera2` at 640×480. Converts frames from RGB to BGR for OpenCV compatibility.
Inference: YOLO runs every 3 frames (to maintain a smooth stream) at `imgsz=320` with `conf=0.5`. These values are a performance trade-off and can be adjusted.
Recording: Saves `.avi` files using MJPG codec at 10 FPS, up to a maximum of 10 minutes per session. Recording can be paused and resumed; elapsed time is accumulated across pauses.
Logging: Each recording session generates a `run_<timestamp>_log.txt` file with timestamped object detections.
API Endpoints
Route	Method	Description
`/`	GET	Web dashboard (HTML page)
`/video_feed`	GET	MJPEG stream of the camera
`/detections`	GET	JSON list of currently detected objects
`/start`	GET	Start or resume recording
`/stop`	GET	Pause recording
---
Model Weights Configuration
> ⚠️ **Weights are not included in this repository and must be configured before running.**
1. Base Model Weights (for training)
In `train_boxes.py`:
```python
model = YOLO("yolov8n.pt")  # ← Download from Ultralytics and place in ML_train_code/
```
Download base weights from Ultralytics and place the file in the `ML_train_code/` directory, or provide the full path.
2. Trained Weights (for inference)
In `test_boxes.py`:
```python
model = YOLO("runs/detect/train/weights/best.pt")  # ← Update to your training run output
```
In `Rasberry_pi_code/main.py`:
```python
model = YOLO("runs/detect/train/weights/best.pt")  # ← Replace with path to your trained best.pt
```
After training, find your best weights at:
```
ML_train_code/runs/detect/trainN/weights/best.pt
```
where `N` is the run number. Copy `best.pt` to the Raspberry Pi and update the path in `main.py`.
Recommended workflow:
Train on a PC or GPU machine using `train_boxes.py`
Copy the output `best.pt` to your Raspberry Pi
Update the `YOLO(...)` path in `main.py` to point to it
---
Setup & Installation
ESP32 (Arduino IDE)
Install Arduino IDE and add ESP32 board support
Install the `ESP32Servo` library via Library Manager
Open `Esp_code/main.ino`
Select your ESP32 board and COM port
Upload the sketch
ML Training (PC / GPU machine)
```bash
pip install ultralytics opencv-python
```
Place your dataset in the folder specified by `path:` in `dataset.yaml` (default: `dataset/`). Then run:
```bash
cd ML_train_code
python train_boxes.py
```
Raspberry Pi
```bash
pip install flask picamera2 ultralytics opencv-python
```
Copy your trained `best.pt` to the Pi and update the model path in `main.py` (see Model Weights Configuration).
---
Running the Project
Start the ESP32
Power on the robot. The ESP32 boots and waits 3 seconds before starting. It will begin line following automatically and print sensor state to Serial at 9600 baud for debugging.
Start the Raspberry Pi Server
```bash
cd Rasberry_pi_code
python main.py
```
The server starts on port `5000`. Open a browser on any device connected to the same network and go to:
```
http://<raspberry-pi-ip>:5000
```
Test ML Model (PC only, no Pi needed)
```bash
cd ML_train_code
python test_boxes.py
```
---
Dashboard Usage
The web dashboard at `http://<pi-ip>:5000` provides:
Live video feed with YOLO bounding boxes overlaid
Detected Objects panel — updates every second via polling
Start Recording button — begins saving video and detection log
Pause Recording button — pauses the session (elapsed time is preserved for resume)
On-screen overlays show:
Elapsed recorded time
Remaining time before the 10-minute limit
Current recording status (`Recording` / `Paused`)
---
Dataset & Classes
The model detects 18 classes across several categories:
Index	Class Name	Category
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
Labels are in YOLO bounding box format (`.txt` files with normalized `class cx cy w h` per line).
---
Tuning & Configuration
ESP32 — Speed & Timing
```cpp
const int baseSpeed = 85;           // Straight-line speed (0–255)
const int turnSpeed = 135;          // Gentle curve outer wheel
const int innerWheelSpeed = 75;     // Gentle curve inner wheel
const int sharpTurnSpeed = 160;     // Sharp turn outer wheel
const int sharpReverseSpeed = 100;  // Sharp turn inner wheel (reverse)
```
Increase `baseSpeed` for faster movement; lower `turnSpeed` for tighter curve tracking. Adjust the `delay()` values in `execute180Turn()` and sharp-turn cases to suit your chassis dimensions.
The lost-line sweep timeouts can also be tuned:
```cpp
const unsigned long sweepTimeoutStraight = 100;  // ms before active scan (was going straight)
const unsigned long sweepTimeoutTurn = 700;       // ms before active scan (was mid-turn)
```
Raspberry Pi — Inference Performance
```python
results = model(frame, imgsz=320, conf=0.5)
```
`imgsz=320` — Smaller image = faster inference. Increase to `640` for higher accuracy at the cost of FPS
`conf=0.5` — Minimum confidence to log a detection. Raise to reduce false positives
`if frame_count % 3 == 0:` — Inference runs every 3rd frame. Lower divisor for more frequent detection, higher for smoother stream
Raspberry Pi — Recording
```python
max_record_time = 600   # 10 minutes. Adjust as needed (seconds)
fps = 10                # Recorded video FPS
```
---
Troubleshooting
Robot drifts on straight line  
Calibrate your IR sensor array height and threshold. Adjust `baseSpeed` and `innerWheelSpeed` for your specific motors.
Ultrasonic sensor gives erratic readings  
The `pulseIn` timeout is set to `20000 µs`. If sensors read `999` (timeout fallback) frequently, check wiring and ensure there are no ground loops between trigger and echo pins.
180° turn doesn't complete / overshoots  
The turn uses dynamic center-sensor re-acquisition — if the line is faint or the sensor array is too high, it may not detect the line behind. Lower the sensor array or adjust the `sharpTurnSpeed` and `sharpReverseSpeed` constants.
YOLO inference is too slow on Raspberry Pi  
Reduce `imgsz` to `160` or `224`, and increase the frame-skip divisor (e.g., `% 5` instead of `% 3`). Consider using a quantized or INT8 model.
`picamera2` fails to start  
Ensure the camera interface is enabled in `raspi-config` and that no other process is using the camera. Run `libcamera-hello` to verify hardware.
`best.pt` not found  
Verify the path string in `YOLO(...)` is correct and the file exists on the Pi. Use an absolute path if needed:
```python
model = YOLO("/home/pi/weights/best.pt")
```
Video stream is blank in browser  
Check that Flask is binding on `0.0.0.0` (it is by default) and that the firewall allows port 5000. Try accessing via IP directly rather than hostname.
Side obstacle detection triggers while turning  
The side sensors are intentionally frozen for 2 seconds after a side-obstacle event. If false triggers persist during 180° turns, they are suppressed by the dynamic `frontFreezeUntil / leftFreezeUntil / rightFreezeUntil` logic automatically.
