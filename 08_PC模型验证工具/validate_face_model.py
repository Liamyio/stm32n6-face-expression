"""
RFB-320 INT8 人脸检测模型 — PC 端精度验证
用法: python validate_face_model.py
      python validate_face_model.py --image your_photo.jpg
      python validate_face_model.py --camera 0

需要: pip install opencv-python onnxruntime numpy
"""
import cv2
import numpy as np
import onnxruntime as ort
import argparse
import os


# ============================================================
# 模型参数 (来自 stedgeai 翻译报告)
# ============================================================
MODEL_INPUT_SIZE = (320, 240)       # W, H
MODEL_INPUT_SCALE = 0.0078125       # 1/128
MODEL_INPUT_ZP = -1

OUTPUT_SCORE_SCALE = 0.00390625     # 1/256
OUTPUT_BOX_SCALE = 0.0060765594

NUM_CANDIDATES = 4420
CONF_THRESHOLD = 0.5
IOU_THRESHOLD = 0.45
MAX_FACES = 20


def preprocess_image(img_bgr):
    """
    BGR 图像 → INT8 模型输入 (int8_t [1, 3, 240, 320] CHW)
    归一化: 先做 x/127.5 - 1.0 得到 float [-1,1], 再量化到 int8 Q(1/128, -1)
       i8 = round(fval / 0.0078125) + (-1) = round(fval * 128) - 1
    """
    # 缩放
    resized = cv2.resize(img_bgr, MODEL_INPUT_SIZE, interpolation=cv2.INTER_LINEAR)
    # BGR → RGB
    rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
    # 归一化 float [-1, 1]
    f_normalized = (rgb.astype(np.float32) / 127.5) - 1.0
    # 量化到 int8
    i8 = (f_normalized * 128.0) - 1.0
    i8 = np.clip(np.round(i8), -128, 127).astype(np.int8)
    # HWC → CHW, add batch dim
    chw = np.transpose(i8, (2, 0, 1))
    batch = np.expand_dims(chw, axis=0)
    return batch, rgb


def decode_outputs(scores_i8, boxes_i8):
    """
    INT8 输出 → float scores & boxes (cx, cy, w, h)
    scores: [1, 2, 4420] → 反量化
    boxes:  [1, 4, 4420] → 反量化
    """
    scores_f = scores_i8.astype(np.float32) * OUTPUT_SCORE_SCALE
    boxes_f  = boxes_i8.astype(np.float32)  * OUTPUT_BOX_SCALE
    return scores_f, boxes_f


def nms_and_decode(scores_f, boxes_f, conf_threshold=0.5):
    """
    NMS + 坐标解码 (cx,cy,w,h) → (x1,y1,x2,y2)
    scores_f: [1, 2, 4420] — 第二维 index 1 是人脸置信度
    boxes_f:  [1, 4, 4420] — cx, cy, w, h
    """
    face_scores = scores_f[0, :, 1]              # [4420] — column index 1 = face
    boxes_t = boxes_f[0, :, :]                    # [4420, 4]

    # 第一步: 过滤 + 解码
    candidates = []
    for i in range(NUM_CANDIDATES):
        score = face_scores[i]
        if score < conf_threshold:
            continue

        cx = boxes_t[i, 0]
        cy = boxes_t[i, 1]
        w  = boxes_t[i, 2]
        h  = boxes_t[i, 3]

        x1 = cx - w * 0.5
        y1 = cy - h * 0.5
        x2 = cx + w * 0.5
        y2 = cy + h * 0.5

        # 裁剪到 [0, 1]
        x1 = max(0.0, min(1.0, x1))
        y1 = max(0.0, min(1.0, y1))
        x2 = max(0.0, min(1.0, x2))
        y2 = max(0.0, min(1.0, y2))

        if w <= 0 or h <= 0 or x2 <= x1 or y2 <= y1:
            continue

        candidates.append({'x1': x1, 'y1': y1, 'x2': x2, 'y2': y2,
                          'score': float(score)})

    if not candidates:
        return []

    # 第二步: 按置信度降序排序
    candidates.sort(key=lambda x: x['score'], reverse=True)

    # 第三步: NMS
    def iou(a, b):
        ix1 = max(a['x1'], b['x1'])
        iy1 = max(a['y1'], b['y1'])
        ix2 = min(a['x2'], b['x2'])
        iy2 = min(a['y2'], b['y2'])
        iw = ix2 - ix1
        ih = iy2 - iy1
        if iw <= 0 or ih <= 0:
            return 0.0
        inter = iw * ih
        area_a = (a['x2'] - a['x1']) * (a['y2'] - a['y1'])
        area_b = (b['x2'] - b['x1']) * (b['y2'] - b['y1'])
        return inter / (area_a + area_b - inter)

    kept = []
    for c in candidates:
        keep = True
        for k in kept:
            if iou(c, k) >= IOU_THRESHOLD:
                keep = False
                break
        if keep:
            kept.append(c)
            if len(kept) >= MAX_FACES:
                break

    return kept


def draw_results(img, faces, elapsed_ms):
    """在图像上画检测结果"""
    h, w = img.shape[:2]
    img = img.copy()

    for i, face in enumerate(faces):
        x1 = int(face['x1'] * w)
        y1 = int(face['y1'] * h)
        x2 = int(face['x2'] * w)
        y2 = int(face['y2'] * h)

        cv2.rectangle(img, (x1, y1), (x2, y2), (0, 255, 0), 2)
        cv2.circle(img, (x1, y1), 4, (0, 255, 0), -1)

        text = f"Face #{i+1}: {face['score']:.2f}"
        cv2.putText(img, text, (x1, y1 - 8),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

    # 底栏
    cv2.rectangle(img, (0, h - 30), (400, h), (0, 0, 0), -1)
    cv2.putText(img, f"Faces detected: {len(faces)}  ({elapsed_ms:.1f}ms)",
                (10, h - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6,
                (255, 255, 255), 1)

    return img


def run_model(onnx_path, input_i8):
    """运行模型，返回 scores_f, boxes_f"""
    import time
    session = ort.InferenceSession(onnx_path, providers=['CPUExecutionProvider'])
    iname = session.get_inputs()[0].name
    t0 = time.time()

    # ONNX Runtime CPU 不支持 int8 ops，需要先反量化到 float
    # 实际板端 NPU 直接跑 int8。这里我们模拟: int8 → float → ONNX Runtime
    inputs_f = (input_i8.astype(np.float32) - MODEL_INPUT_ZP) * MODEL_INPUT_SCALE

    outputs = session.run(None, {iname: inputs_f})
    t1 = time.time()

    scores_f, boxes_f = outputs[0], outputs[1]
    return scores_f, boxes_f, (t1 - t0) * 1000


def main():
    parser = argparse.ArgumentParser(description='RFB-320 INT8 人脸检测验证')
    parser.add_argument('--onnx', type=str,
                        default='E:/STM32N6/02_最终模型文件/rfb320_int8_qdq.onnx',
                        help='INT8 QDQ ONNX 路径')
    parser.add_argument('--image', type=str, default=None,
                        help='测试图片路径 (不指定则用 Haar Cascade 生成测试样本)')
    parser.add_argument('--camera', type=int, default=-1,
                        help='摄像头设备号 (>=0 开启实时测试)')
    parser.add_argument('--conf', type=float, default=0.5,
                        help='置信度阈值')
    args = parser.parse_args()

    conf_thresh = args.conf

    # 加载模型
    if not os.path.exists(args.onnx):
        print(f"ERROR: ONNX 不存在: {args.onnx}")
        return
    print(f"模型: {args.onnx}")

    session = ort.InferenceSession(args.onnx, providers=['CPUExecutionProvider'])
    iname = session.get_inputs()[0].name
    onames = [o.name for o in session.get_outputs()]
    print(f"  输入: {iname} shape={session.get_inputs()[0].shape}")
    print(f"  输出: {onames}")

    # 摄像头模式
    if args.camera >= 0:
        cap = cv2.VideoCapture(args.camera)
        if not cap.isOpened():
            print(f"ERROR: 无法打开摄像头 {args.camera}")
            return
        print(f"摄像头已打开, 按 'q' 退出, 's' 保存截图\n")

        while True:
            ret, frame = cap.read()
            if not ret:
                break
            frame = cv2.flip(frame, 1)

            prep, _ = preprocess_image(frame)
            scores_f, boxes_f, elapsed = run_model(args.onnx, prep)
            faces = nms_and_decode(scores_f, boxes_f, conf_thresh)

            result = draw_results(frame, faces, elapsed)
            cv2.imshow('RFB-320 Face Detection Validation', result)

            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                break
            elif key == ord('s'):
                cv2.imwrite('face_detect_snapshot.jpg', result)
                print("截图已保存: face_detect_snapshot.jpg")

        cap.release()
        cv2.destroyAllWindows()
        return

    # 单图模式
    if args.image and os.path.exists(args.image):
        img = cv2.imread(args.image)
        print(f"测试图片: {args.image} ({img.shape[1]}x{img.shape[0]})")
    else:
        # 使用 Haar Cascade 从摄像头抓一帧含人脸的图
        print("未指定测试图, 用 Haar Cascade 从摄像头抓一帧...")
        cap = cv2.VideoCapture(0)
        if not cap.isOpened():
            print("ERROR: 摄像头不可用, 请用 --image 指定图片路径")
            return

        cascade_path = cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
        face_cascade = cv2.CascadeClassifier(cascade_path)

        img = None
        for attempt in range(30):
            ret, frame = cap.read()
            if not ret:
                continue
            frame = cv2.flip(frame, 1)
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
            faces_haar = face_cascade.detectMultiScale(gray, 1.1, 5, minSize=(80, 80))
            if len(faces_haar) > 0:
                img = frame
                print(f"抓到一帧, Haar 检测到 {len(faces_haar)} 张脸")
                break
            else:
                print(f"等待人脸... {attempt + 1}/30", end='\r')
        cap.release()

    if img is None:
        print("\nERROR: 未获取到含人脸的图片")
        return

    # 推理
    prep, rgb = preprocess_image(img)
    scores_f, boxes_f, elapsed = run_model(args.onnx, prep)
    faces = nms_and_decode(scores_f, boxes_f)

    print(f"\n推理耗时: {elapsed:.1f}ms")
    print(f"检测到 {len(faces)} 个人脸:")
    for i, f in enumerate(faces):
        print(f"  #{i+1}: score={f['score']:.3f}  "
              f"({f['x1']:.2f},{f['y1']:.2f})-({f['x2']:.2f},{f['y2']:.2f})")

    # 显示结果
    result = draw_results(img, faces, elapsed)
    cv2.imshow('Validation Result (press any key to exit)', result)
    cv2.waitKey(0)
    cv2.destroyAllWindows()

    # 保存
    out_path = 'face_detect_result.jpg'
    cv2.imwrite(out_path, result)
    print(f"\n结果已保存: {out_path}")


if __name__ == "__main__":
    main()
