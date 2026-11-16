"""
RFB-320 INT8 人脸检测 — 快速 CLI 精度测试
"""
import onnxruntime as ort
import numpy as np
from PIL import Image
import sys
import time

MODEL_PATH = "E:/STM32N6/02_最终模型文件/rfb320_int8_qdq.onnx"
MODEL_IN = (320, 240)
INPUT_SCALE = 0.0078125
INPUT_ZP = -1
SCORE_SCALE = 0.00390625
BOX_SCALE = 0.0060765594
CONF_THRESH = 0.3
IOU_THRESH = 0.45
NUM_ANCHORS = 4420


def preprocess_pil(img_path):
    img = Image.open(img_path).convert("RGB")
    img = img.resize(MODEL_IN, Image.BILINEAR)
    arr = np.array(img, dtype=np.float32)
    # 归一化 [-1, 1]
    arr = arr / 127.5 - 1.0
    # 量化 int8
    arr = (arr * 128.0) - 1.0
    arr = np.clip(np.round(arr), -128, 127).astype(np.int8)
    # HWC → CHW → batch
    arr = np.transpose(arr, (2, 0, 1))
    return np.expand_dims(arr, axis=0)


def nms(scores, boxes):
    face_s = scores[0, :, 1]
    boxes_t = boxes[0, :, :]

    cand = []
    for i in range(NUM_ANCHORS):
        s = face_s[i]
        if s < CONF_THRESH:
            continue
        cx, cy, w, h = boxes_t[i, 0], boxes_t[i, 1], boxes_t[i, 2], boxes_t[i, 3]
        x1 = max(0, min(1, cx - w * 0.5))
        y1 = max(0, min(1, cy - h * 0.5))
        x2 = max(0, min(1, cx + w * 0.5))
        y2 = max(0, min(1, cy + h * 0.5))
        if w <= 0 or h <= 0 or x2 <= x1 or y2 <= y1:
            continue
        cand.append({'score': s, 'x1': x1, 'y1': y1, 'x2': x2, 'y2': y2})

    if not cand:
        return []
    cand.sort(key=lambda x: x['score'], reverse=True)

    kept = []
    for c in cand:
        ok = True
        for k in kept:
            ix1 = max(c['x1'], k['x1'])
            iy1 = max(c['y1'], k['y1'])
            ix2 = min(c['x2'], k['x2'])
            iy2 = min(c['y2'], k['y2'])
            iw, ih = ix2 - ix1, iy2 - iy1
            if iw > 0 and ih > 0:
                inter = iw * ih
                a_a = (c['x2'] - c['x1']) * (c['y2'] - c['y1'])
                a_b = (k['x2'] - k['x1']) * (k['y2'] - k['y1'])
                if inter / (a_a + a_b - inter) >= IOU_THRESH:
                    ok = False
                    break
        if ok:
            kept.append(c)
            if len(kept) >= 20:
                break
    return kept


if __name__ == "__main__":
    img_path = sys.argv[1] if len(sys.argv) > 1 else None
    if not img_path:
        print("用法: python quick_test.py <图片路径> [置信度阈值]")
        print("      python quick_test.py camera")
        sys.exit(1)

    if img_path == "camera":
        import cv2
        cap = cv2.VideoCapture(0)
        for _ in range(30):
            ret, f = cap.read()
            if not ret: continue
            gray = cv2.cvtColor(f, cv2.COLOR_BGR2GRAY)
            fc = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')
            faces = fc.detectMultiScale(gray, 1.1, 5, minSize=(80, 80))
            if len(faces) > 0:
                cv2.imwrite("E:/STM32N6/08_PC模型验证工具/camera_snap.jpg", f)
                img_path = "E:/STM32N6/08_PC模型验证工具/camera_snap.jpg"
                print(f"抓到含 {len(faces)} 张脸的照片")
                break
            print(f"等待人脸 {_+1}/30", end="\r")
        cap.release()

    if img_path is None:
        print("未获取到照片")
        sys.exit(1)

    if len(sys.argv) > 2:
        CONF_THRESH = float(sys.argv[2])

    # 预处理
    inp = preprocess_pil(img_path)
    # 反量化到 float (ONNX Runtime CPU 不支持 int8 ops)
    inp_f = (inp.astype(np.float32) - INPUT_ZP) * INPUT_SCALE

    session = ort.InferenceSession(MODEL_PATH, providers=['CPUExecutionProvider'])
    t0 = time.time()
    out = session.run(None, {'input': inp_f})
    dt = (time.time() - t0) * 1000

    scores_f = out[0] * SCORE_SCALE
    boxes_f = out[1] * BOX_SCALE

    faces = nms(scores_f, boxes_f)

    print(f"\n{'='*50}")
    print(f"推理耗时: {dt:.1f}ms  |  阈值: {CONF_THRESH}")
    print(f"检测到 {len(faces)} 个人脸:")
    for i, f in enumerate(faces):
        print(f"  #{i+1}: score={f['score']:.3f}  "
              f"box=({f['x1']:.2f},{f['y1']:.2f})-({f['x2']:.2f},{f['y2']:.2f})")
    print(f"{'='*50}")
