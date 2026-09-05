from ultralytics import YOLO

# 加载预训练模型（自动下载或使用本地文件）
model = YOLO("yolov8n.pt")  # 可换成 yolov8s.pt, yolov8m.pt 等

# 对单张图片推理
results = model("bus.jpg")  # 可以是 URL 或本地路径

# 打印检测结果
for r in results:
    # 显示检测到的物体信息
    boxes = r.boxes.xyxy.cpu().numpy()   # 边界框坐标
    confs = r.boxes.conf.cpu().numpy()   # 置信度
    cls_ids = r.boxes.cls.cpu().numpy()  # 类别ID
    for box, conf, cls in zip(boxes, confs, cls_ids):
        print(f"类别: {model.names[int(cls)]}, 置信度: {conf:.2f}, 框: {box}")

# 保存结果图片（带标注）
results[0].save("result.jpg")