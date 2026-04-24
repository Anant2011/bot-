# 🚀 Rover Vision System using YOLOv8

**A real-time object detection system with live video streaming, recording, and a web dashboard, designed for robotics and embedded vision applications.**

---

## 📌 Project Overview

This project implements an end-to-end computer vision pipeline using YOLOv8 integrated with a Flask-based dashboard.

The system:

* Captures live video from a camera
* Performs real-time object detection
* Streams annotated frames to a browser
* Records video sessions
* Logs detected objects with timestamps

---

## ⚡ Features

* 🔍 Real-time object detection using YOLOv8
* 📡 Live video streaming via Flask
* 🎥 Video recording with start/stop control
* 📝 Detection logging with timestamps
* ⏱️ Recording timer and duration tracking
* 🖥️ Browser-based dashboard

---

## 🛠️ Tech Stack

* **Language:** Python
* **Libraries:** OpenCV, Flask, Ultralytics YOLOv8
* **Hardware:** Webcam / Raspberry Pi Camera

---

## 📂 Project Structure

```
.
├── main.py              # Flask server + live detection + recording
├── train_boxes.py       # YOLO model training script
├── test_boxes.py        # Real-time detection using webcam
├── dataset.yaml         # Dataset configuration
├── runs/                # Training outputs (weights, logs)
```

---

## 🧠 How It Works

### 1. Model Training

```python
from ultralytics import YOLO

model = YOLO("yolov8n.pt")
model.train(data="dataset.yaml", epochs=2, imgsz=640)
```

---

### 2. Real-Time Detection

```python
results = model(frame, conf=0.0032)
annotated = results[0].plot()
```

---

### 3. Flask Dashboard

* `/video_feed` → live stream
* `/start` → start recording
* `/stop` → pause recording
* `/detections` → detected objects API

---

## ▶️ How to Run

### 1. Install dependencies

```bash
pip install ultralytics opencv-python flask picamera2
```

---

### 2. Train the model

```bash
python train_boxes.py
```

---

### 3. Test detection (optional)

```bash
python test_boxes.py
```

---

### 4. Run full system

```bash
python main.py
```

Open browser:

```
http://localhost:5000
```

---

## 📊 Output

* Live video with bounding boxes
* Detected object names on dashboard
* Recorded `.avi` video files
* Detection logs with timestamps

---

## 📚 Concepts Used

* Object Detection (YOLOv8)
* Computer Vision (OpenCV)
* Web Streaming (Flask)
* Real-time Systems

---

## 🔧 Future Improvements

* Add object tracking (DeepSORT)
* Improve model accuracy with more training
* Add alert/notification system
* Deploy on embedded hardware (Jetson Nano / Raspberry Pi)

---

## 👤 Author

**Anant Chauhan**
B.Tech ECE, IIT Guwahati
