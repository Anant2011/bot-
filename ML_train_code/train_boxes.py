from ultralytics import YOLO

# 1. Load the YOLOv8n Object Detection model
model = YOLO("yolov8n.pt")

# 2. Train the model on your bounding box dataset
# You will need a .yaml file that points to your images/labels folders
model.train(
    data="dataset.yaml", 
    epochs=2, 
    imgsz=640  # Detection models usually need higher resolution than classifiers
)
