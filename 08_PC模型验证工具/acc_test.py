#!/usr/bin/env python3
"""Face detection accuracy test — with PIL-based image loading"""
from PIL import Image
import numpy as np
import onnxruntime as ort
import sys

MODEL = "E:/STM32N6/02_最终模型文件/rfb320_int8_qdq.onnx"
CONF_THRESH = 0.5
IOU_THRESH = 0.45
NUM_ANCHORS = 4420

def preprocess(img_path):
    img = Image.open(img_path).convert("RGB")
    orig_w, orig_h = img.size
    img = img.resize((320, 240), Image.BILINEAR)
    arr = np.array(img, dtype=np.float32) / 127.5 - 1.0
    arr = np.transpose(arr, (2, 0, 1))
    return np.expand_dims(arr, axis=0), orig_w, orig_h

def nms(scores_raw, boxes_raw):
    """scores_raw: (1, 4420, 2), boxes_raw: (1, 4420, 4)"""
    face_s = scores_raw[0, :, 1]
    boxes_t = boxes_raw[0, :, :]

    cand = []
    for i in range(NUM_ANCHORS):
        s = float(face_s[i])
        if s < CONF_THRESH:
            continue
        cx, cy, w, h = boxes_t[i, 0], boxes_t[i, 1], boxes_t[i, 2], boxes_t[i, 3]
        x1 = cx - w * 0.5
        y1 = cy - h * 0.5
        x2 = cx + w * 0.5
        y2 = cy + h * 0.5
        x1 = max(0.0, min(1.0, x1))
        y1 = max(0.0, min(1.0, y1))
        x2 = max(0.0, min(1.0, x2))
        y2 = max(0.0, min(1.0, y2))
        if w <= 0 or h <= 0 or x2 <= x1 or y2 <= y1:
            continue
        cand.append({'score': s, 'x1': x1, 'y1': y1, 'x2': x2, 'y2': y2})

    if not cand:
        return []
    cand.sort(key=lambda x: x['score'], reverse=True)

    def iou(a, b):
        ix1 = max(a['x1'], b['x1']); iy1 = max(a['y1'], b['y1'])
        ix2 = min(a['x2'], b['x2']); iy2 = min(a['y2'], b['y2'])
        iw = ix2 - ix1; ih = iy2 - iy1
        if iw <= 0 or ih <= 0: return 0.0
        inter = iw * ih
        a_a = (a['x2'] - a['x1']) * (a['y2'] - a['y1'])
        a_b = (b['x2'] - b['x1']) * (b['y2'] - b['y1'])
        return inter / (a_a + a_b - inter)

    kept = []
    for c in cand:
        ok = True
        for k in kept:
            if iou(c, k) >= IOU_THRESH:
                ok = False
                break
        if ok:
            kept.append(c)
            if len(kept) >= 20:
                break
    return kept

if __name__ == "__main__":
    img_path = sys.argv[1] if len(sys.argv) > 1 else "E:/STM32N6/微信图片_20260628015801_408_72.jpg"
    if len(sys.argv) > 2:
        CONF_THRESH = float(sys.argv[2])

    import time
    inp, ow, oh = preprocess(img_path)
    session = ort.InferenceSession(MODEL, providers=['CPUExecutionProvider'])
    t0 = time.time()
    out = session.run(None, {'input': inp.astype(np.float32)})
    dt = (time.time() - t0) * 1000

    faces = nms(out[0], out[1])

    print(f"\n推理耗时: {dt:.1f}ms  |  阈值: {CONF_THRESH}")
    print(f"图片: {img_path} ({ow}x{oh})")
    print(f"检测到 {len(faces)} 个人脸:")
    for i, f in enumerate(faces):
        px1 = int(f['x1'] * ow); py1 = int(f['y1'] * oh)
        px2 = int(f['x2'] * ow); py2 = int(f['y2'] * oh)
        print(f"  #{i+1}: score={f['score']:.3f}  "
              f"({px1},{py1})-({px2},{py2})  "
              f"({f['x1']:.2f},{f['y1']:.2f})-({f['x2']:.2f},{f['y2']:.2f})")
