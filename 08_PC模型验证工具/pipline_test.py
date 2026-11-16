#!/usr/bin/env python3
"""双模型全链路串行验证"""
from PIL import Image
import numpy as np
import onnxruntime as ort
import sys, time

FACE_MODEL = "E:/STM32N6/02_最终模型文件/rfb320_int8_qdq.onnx"
EXPR_MODEL = "E:/STM32N6/01_双模型主工程/Appli/Models/expr_net/best_model_int8(1)_OE_3_3_1.onnx"
CONF_THRESH = 0.5
EMOTIONS = ["Anger", "Disgust", "Fear", "Happiness", "Neutral", "Sadness", "Surprise"]

# face model quantization params (int8 QDQ)
FACE_IN_SCALE = 0.0078125; FACE_IN_ZP = -1

# ResNet18 表情 INT8: scale=0.007843138, zp=0
EXPR_IN_SCALE = 0.007843138; EXPR_IN_ZP = 0
EXPR_OUT_SCALE = 0.019689815; EXPR_OUT_ZP = -32

def preprocess_face_model(img_path):
    img = Image.open(img_path).convert("RGB")
    ow, oh = img.size
    img = img.resize((320, 240), Image.BILINEAR)
    arr = np.array(img, dtype=np.float32) / 127.5 - 1.0
    # quantize to int8: i8 = round(fval / 0.0078125) + (-1) = round(fval * 128) - 1
    arr_i8 = np.clip(np.round(arr * 128.0 - 1.0), -128, 127).astype(np.int8)
    arr_i8 = np.transpose(arr_i8, (2, 0, 1))
    return np.expand_dims(arr_i8, axis=0), ow, oh, img

def nms_face_detect(scores_raw, boxes_raw):
    face_s = scores_raw[0, :, 1]
    bt = boxes_raw[0, :, :]
    cand = []
    for i in range(4420):
        s = float(face_s[i])
        if s < CONF_THRESH: continue
        cx, cy, w, h = bt[i, 0], bt[i, 1], bt[i, 2], bt[i, 3]
        # Box format: (cx, cy, w, h) — center coordinates normalized 0-1
        x1, y1 = cx - w*0.5, cy - h*0.5
        x2, y2 = cx + w*0.5, cy + h*0.5
        if w <= 0 or h <= 0 or x2 <= x1 or y2 <= y1: continue
        cand.append({'score': s, 'x1': x1, 'y1': y1, 'x2': x2, 'y2': y2})
        cand.append({'score': s, 'x1': x1, 'y1': y1, 'x2': x2, 'y2': y2})
    if not cand: return []
    cand.sort(key=lambda x: x['score'], reverse=True)
    kept = []
    for c in cand:
        ok = True
        for k in kept:
            ix1, iy1 = max(c['x1'], k['x1']), max(c['y1'], k['y1'])
            ix2, iy2 = min(c['x2'], k['x2']), min(c['y2'], k['y2'])
            iw, ih = ix2 - ix1, iy2 - iy1
            if iw > 0 and ih > 0:
                inter = iw * ih
                a_a = (c['x2'] - c['x1']) * (c['y2'] - c['y1'])
                a_b = (k['x2'] - k['x1']) * (k['y2'] - k['y1'])
                if inter / (a_a + a_b - inter) >= 0.45:
                    ok = False; break
        if ok:
            kept.append(c)
            if len(kept) >= 20: break
    return kept

def crop_and_preprocess_expr(pil_img, face_box):
    """裁剪人脸 → 缩放 224×224 → float [-1,1] → int8"""
    ow, oh = pil_img.size
    fx1 = max(0, int(face_box['x1'] * ow)); fy1 = max(0, int(face_box['y1'] * oh))
    fx2 = min(oh if 'y' not in face_box else ow, int(face_box['x2'] * ow))
    fy2 = min(oh, int(face_box['y2'] * oh))
    x1, y1, x2, y2 = fx1, fy1, fx2, fy2
    # Add 20% margin
    m = 0.2
    x1 = max(0, int(x1 - (x2 - x1) * m)); y1 = max(0, int(y1 - (y2 - y1) * m))
    x2 = min(ow, int(x2 + (x2 - x1) * m * 1.5)); y2 = min(oh, int(y2 + (y2 - y1) * m * 1.5))
    # Ensure square (make width = height)
    w = x2 - x1; h = y2 - y1
    if w < h:
        diff = (h - w) // 2; x1 = max(0, x1 - diff); x2 = min(ow, x2 + diff)
    else:
        diff = (w - h) // 2; y1 = max(0, y1 - diff); y2 = min(oh, y2 + diff)

    # Expand face crop by 20% margin for context, then resize
    margin = 0.2
    fx1 = x1 - (x2 - x1) * margin; fy1 = y1 - (y2 - y1) * margin
    fx2 = x2 + (x2 - x1) * margin; fy2 = y2 + (y2 - y1) * margin
    fx1, fy1 = max(0, int(fx1)), max(0, int(fy1))
    fx2, fy2 = min(ow, int(fx2)), min(oh, int(fy2))
    if fx2 <= fx1 or fy2 <= fy1:
        return None

    face = pil_img.crop((fx1, fy1, fx2, fy2))
    face = face.resize((224, 224), Image.LANCZOS)
    # Also save for debugging
    fname = img_path.replace('\\', '/').split('/')[-1].split('.')[0]
    face.save(f'E:/STM32N6/08_PC模型验证工具/expr_crop_{fname}.jpg', 'JPEG', quality=85)

    arr = np.array(face, dtype=np.float32) / 127.5 - 1.0
    # quantize: i8 = round(fval / 0.007843138) + 0 = round(fval * 127.5)
    arr_i8 = np.clip(np.round(arr * 127.5), -128, 127).astype(np.int8)
    arr_i8 = np.transpose(arr_i8, (2, 0, 1))
    return np.expand_dims(arr_i8, axis=0)

if __name__ == "__main__":
    img_path = sys.argv[1] if len(sys.argv) > 1 else "E:/STM32N6/微信图片_20260628015801_408_72.jpg"

    face_sess = ort.InferenceSession(FACE_MODEL, providers=['CPUExecutionProvider'])
    expr_sess = ort.InferenceSession(EXPR_MODEL, providers=['CPUExecutionProvider'])

    inp_f, ow, oh, pil_img = preprocess_face_model(img_path)
    # ONNX Runtime CPU 不支持 int8 ops — 反量化到 float 送入
    inp_fp32 = (inp_f.astype(np.float32) - FACE_IN_ZP) * FACE_IN_SCALE

    t0 = time.time()
    out_f = face_sess.run(None, {'input': inp_fp32})
    faces = nms_face_detect(out_f[0], out_f[1])
    t1 = time.time()

    print(f"\n{'='*60}")
    print(f"图片: {img_path} ({ow}x{oh})")
    print(f"人脸检测: {t1-t0:.3f}s, 检测到 {len(faces)} 人")
    print(f"{'='*60}")

    for i, f in enumerate(faces):
        expr_in = crop_and_preprocess_expr(pil_img, f)
        if expr_in is None:
            print(f"\n  #{i+1}: 裁剪失败")
            continue

        # 表情模型要求 int8 输入！ 送 int8
        t2 = time.time()
        out_e = expr_sess.run(None, {'Input_0_out_0': expr_in.astype(np.int8)})[0]
        t3 = time.time()

        # 反量化: logit_f = (out_i8 + 32) * scale
        logits = (out_e + EXPR_OUT_ZP) * EXPR_OUT_SCALE
        logits = logits[0]  # [1, 7] → [7]
        exp_logits = np.exp(logits - logits.max())
        probs = exp_logits / exp_logits.sum()
        top = np.argmax(logits)

        px1 = int(f['x1'] * ow); py1 = int(f['y1'] * oh)
        px2 = int(f['x2'] * ow); py2 = int(f['y2'] * oh)
        print(f"  #{i+1}: ({px1},{py1})-({px2},{py2}) score={f['score']:.3f}")
        print(f"       表情: {EMOTIONS[top]} ({probs[top]*100:.1f}%) 推理: {(t3-t2)*1000:.1f}ms")
        # 详情行
        parts = []
        for j in range(7):
            parts.append(f"{EMOTIONS[j]}={probs[j]*100:.1f}")
        print(f"       {'  '.join(parts)}")
