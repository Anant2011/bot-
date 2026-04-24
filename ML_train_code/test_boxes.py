import cv2
from ultralytics import YOLO

model = YOLO("runs/detect/train/weights/best.pt")

cap = cv2.VideoCapture(0)  # change index for external camera

if not cap.isOpened():
    print("Cannot open camera")
    exit()

while True:
    success, frame = cap.read()

    if not success:
        print("Error: Failed to grab frame from camera.")
        break

    results = model(frame, verbose=False,conf=0.0032)
    annotated = results[0].plot()

    cv2.imshow("Box Detection", annotated)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        print("Exiting.")
        break

cap.release()
cv2.destroyAllWindows()
