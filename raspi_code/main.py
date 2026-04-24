import cv2
import time
import atexit
from flask import Flask, Response, jsonify
from picamera2 import Picamera2
from ultralytics import YOLO

app = Flask(__name__)

# -----------------------
# Load YOLO model
# -----------------------

model = YOLO("runs/detect/train/weights/best.pt")      #configurable path

# -----------------------
# Camera setup
# -----------------------

picam2 = Picamera2()

config = picam2.create_preview_configuration(
    main={"size": (640,480)}
)

picam2.configure(config)
picam2.start()

time.sleep(2)

frame = picam2.capture_array()
frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)

h,w,_ = frame.shape

# -----------------------
# Recording setup
# -----------------------

fourcc = cv2.VideoWriter_fourcc(*'MJPG')
out = None

recording = False
start_time = None
recorded_time = 0

max_record_time = 600  # 10 minutes

log_file = None
video_filename = None

# -----------------------
# Detection variables
# -----------------------

frame_count = 0
last_detection = None
detected_objects = []

# -----------------------
# Frame generator
# -----------------------

def gen_frames():

    global recording,start_time,recorded_time,out
    global frame_count,last_detection,detected_objects
    global log_file

    while True:

        frame = picam2.capture_array()
        frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)

        frame_count += 1

        # Run detection every 3 frames
        if frame_count % 3 == 0:

            results = model(frame, imgsz=320, conf=0.5)

            detected_objects = []

            timestamp = time.strftime("%H:%M:%S")

            for box in results[0].boxes:

                cls = int(box.cls[0])
                label = model.names[cls]

                detected_objects.append(label)

                if log_file is not None:
                    log_file.write(f"{timestamp} : {label}\n")
                    log_file.flush()

            last_detection = results[0].plot()

        if last_detection is not None:
            frame = last_detection

        elapsed = recorded_time

        if recording:

            elapsed = recorded_time + (time.time() - start_time)

            if elapsed >= max_record_time:

                recording = False
                recorded_time = max_record_time

        if recording and out is not None:
            out.write(frame)

        remaining = max_record_time - elapsed

        if remaining < 0:
            remaining = 0

        timer = f"Recorded: {int(elapsed/60)}m {int(elapsed%60)}s"
        remain = f"Remaining: {int(remaining/60)}m {int(remaining%60)}s"
        status = "Recording" if recording else "Paused"

        cv2.putText(frame,timer,(20,40),
                    cv2.FONT_HERSHEY_SIMPLEX,1,(0,255,0),2)

        cv2.putText(frame,remain,(20,80),
                    cv2.FONT_HERSHEY_SIMPLEX,1,(255,255,0),2)

        cv2.putText(frame,status,(20,120),
                    cv2.FONT_HERSHEY_SIMPLEX,1,(0,0,255),2)

        ret,buffer = cv2.imencode('.jpg',frame)
        frame_bytes = buffer.tobytes()

        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' +
               frame_bytes + b'\r\n')

# -----------------------
# Dashboard page
# -----------------------

@app.route('/')
def index():

    return """
    <html>

    <head>

    <script>

    function updateObjects() {

        fetch('/detections')
        .then(response => response.json())
        .then(data => {

            document.getElementById("objects").innerText =
            data.objects.length ? data.objects.join(", ") : "None";

        });

    }

    setInterval(updateObjects,1000);

    </script>

    </head>

    <body style="font-family:Arial">

    <h1>Rover Vision Dashboard</h1>

    <img src="/video_feed" width="640">

    <h2>Detected Objects</h2>

    <p id="objects">None</p>

    <br>

    <button onclick="fetch('/start')">Start Recording</button>
    <button onclick="fetch('/stop')">Pause Recording</button>

    </body>

    </html>
    """

# -----------------------
# Detection API
# -----------------------

@app.route('/detections')
def detections():

    return jsonify({"objects": detected_objects})

# -----------------------
# Video stream
# -----------------------

@app.route('/video_feed')
def video_feed():

    return Response(gen_frames(),
    mimetype='multipart/x-mixed-replace; boundary=frame')

# -----------------------
# Start recording
# -----------------------

@app.route('/start')
def start_recording():

    global recording,start_time,out
    global log_file,video_filename

    if not recording:

        start_time = time.time()
        recording = True

        timestamp = int(time.time())

        video_filename = f"run_{timestamp}.avi"
        log_filename = f"run_{timestamp}_log.txt"

        fps = 10

        out = cv2.VideoWriter(
            video_filename,
            cv2.VideoWriter_fourcc(*'MJPG'),
            fps,
            (w,h)
        )

        log_file = open(log_filename,"w")

    return "Recording started"

# -----------------------
# Stop recording
# -----------------------

@app.route('/stop')
def stop_recording():

    global recording,recorded_time,start_time
    global log_file

    if recording:

        recorded_time += time.time() - start_time
        recording = False

        if log_file is not None:
            log_file.close()
            log_file = None

    return "Recording paused"

# -----------------------
# Cleanup
# -----------------------

def cleanup():

    picam2.stop()

    if out is not None:
        out.release()

    if log_file is not None:
        log_file.close()

atexit.register(cleanup)

# -----------------------
# Run server
# -----------------------

if __name__ == "__main__":

    app.run(host="0.0.0.0",port=5000)
