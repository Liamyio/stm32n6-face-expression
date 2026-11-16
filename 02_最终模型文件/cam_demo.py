"""
人脸表情识别 — 摄像头实时 Demo
用法: python cam_demo.py --onnx_path best_model.onnx [--camera 0]

需要安装: pip install opencv-python onnxruntime numpy pillow
"""
import cv2
import numpy as np
import onnxruntime as ort
import argparse, os
from collections import deque


CLASS_NAMES = ['angry', 'disgust', 'fear', 'happy', 'neutral', 'sad', 'surprise']
EMOJI_MAP = {
    'angry':    '😠',
    'disgust':  '🤢',
    'fear':     '😨',
    'happy':    '😊',
    'neutral':  '😐',
    'sad':      '😢',
    'surprise': '😲',
}


def parse_args():
    parser = argparse.ArgumentParser(description='人脸表情识别 Demo')
    parser.add_argument('--onnx_path', type=str, default='best_model_int81111.onnx',
                        help='ONNX 模型路径')
    parser.add_argument('--camera', type=int, default=0,
                        help='摄像头设备号 (默认 0)')
    parser.add_argument('--conf_thresh', type=float, default=0.5,
                        help='人脸置信度阈值')
    parser.add_argument('--face_detector', type=str, default='haar',
                        choices=['haar', 'dnn'],
                        help='人脸检测器: haar (内置) / dnn (需下载模型)')
    return parser.parse_args()


def preprocess_face(face_img, target_size=(224, 224)):
    """预处理人脸图像为模型输入格式"""
    # 缩放
    resized = cv2.resize(face_img, target_size, interpolation=cv2.INTER_CUBIC)
    # BGR -> RGB
    rgb = cv2.cvtColor(resized, cv2.COLOR_BGR2RGB)
    # 归一化到 [-1, 1]（与训练一致）
    normalized = (rgb.astype(np.float32) / 127.5) - 1.0
    # HWC -> CHW
    chw = np.transpose(normalized, (2, 0, 1))
    # 添加 batch 维度
    batch = np.expand_dims(chw, axis=0).astype(np.float32)
    return batch


def draw_text_with_bg(img, text, pos, font_scale=0.7, thickness=2,
                      text_color=(255, 255, 255), bg_color=(0, 0, 0)):
    """带背景框的文字绘制"""
    font = cv2.FONT_HERSHEY_SIMPLEX
    (tw, th), baseline = cv2.getTextSize(text, font, font_scale, thickness)
    x, y = pos
    cv2.rectangle(img, (x, y - th - baseline), (x + tw, y + baseline),
                  bg_color, -1)
    cv2.putText(img, text, (x, y), font, font_scale, text_color, thickness)


def process_frame(frame, ort_session, input_name, face_cascade):
    """处理单帧图像：人脸检测 + 表情识别"""
    h, w = frame.shape[:2]

    # ----- 人脸检测 -----
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    faces = face_cascade.detectMultiScale(
        gray, scaleFactor=1.1, minNeighbors=5,
        minSize=(60, 60), flags=cv2.CASCADE_SCALE_IMAGE
    )

    results = []
    for (fx, fy, fw, fh) in faces:
        # 扩大人脸框 20%（让边缘更完整）
        margin = 0.2
        x1 = max(0, int(fx - fw * margin))
        y1 = max(0, int(fy - fh * margin))
        x2 = min(w, int(fx + fw * (1 + margin)))
        y2 = min(h, int(fy + fh * (1 + margin)))

        # 裁剪人脸
        face_roi = frame[y1:y2, x1:x2]
        if face_roi.size == 0:
            continue

        # 预处理 + 推理
        input_tensor = preprocess_face(face_roi)
        outputs = ort_session.run(None, {input_name: input_tensor})[0]  # (1, 7)
        scores = np.squeeze(outputs)  # (7,)

        # softmax 转概率
        exp_scores = np.exp(scores - np.max(scores))
        probs = exp_scores / np.sum(exp_scores)

        pred_idx = np.argmax(scores)
        confidence = probs[pred_idx]
        label = CLASS_NAMES[pred_idx]

        results.append({
            'box': (x1, y1, x2, y2),
            'label': label,
            'confidence': confidence,
        })

    return results


def main():
    args = parse_args()

    # 1. 加载 ONNX 模型
    if not os.path.exists(args.onnx_path):
        print(f"❌ ONNX 模型不存在: {args.onnx_path}")
        print("请先拷贝模型到本地，或用 --onnx_path 指定路径")
        return

    print(f"加载 ONNX 模型: {args.onnx_path}")
    ort_session = ort.InferenceSession(args.onnx_path, providers=['CPUExecutionProvider'])
    input_name = ort_session.get_inputs()[0].name
    print(f"  输入: {input_name} shape={ort_session.get_inputs()[0].shape}")

    # 2. 加载人脸检测器
    cascade_path = cv2.data.haarcascades + 'haarcascade_frontalface_default.xml'
    if not os.path.exists(cascade_path):
        print("❌ 找不到 Haar cascade 文件，请确认 opencv 安装正确")
        return
    face_cascade = cv2.CascadeClassifier(cascade_path)
    print(f"人脸检测器: Haar Cascade")

    # 3. 打开摄像头
    cap = cv2.VideoCapture(args.camera)
    if not cap.isOpened():
        print(f"❌ 无法打开摄像头 (设备 {args.camera})")
        return
    print(f"摄像头已打开 (设备 {args.camera})")
    print(f"按 'q' 退出，按 's' 保存截图\n")

    # 平滑历史（减少抖动）
    smooth_history = deque(maxlen=5)

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # 水平翻转（镜像效果）
        frame = cv2.flip(frame, 1)

        # 推理
        results = process_frame(frame, ort_session, input_name, face_cascade)

        # 绘制结果
        for r in results:
            x1, y1, x2, y2 = r['box']
            label = r['label']
            conf = r['confidence']
            emoji = EMOJI_MAP.get(label, '')

            # 平滑历史（取最近几帧最多的表情）
            smooth_history.append(label)
            if smooth_history:
                smooth_label = max(set(smooth_history), key=smooth_history.count)
            else:
                smooth_label = label

            # 画人脸框（颜色按表情不同）
            color_map = {
                'angry':    (0, 0, 255),    # 红
                'disgust':  (0, 255, 255),  # 黄
                'fear':     (128, 0, 128),  # 紫
                'happy':    (0, 255, 0),    # 绿
                'neutral':  (255, 255, 255),# 白
                'sad':      (255, 0, 0),    # 蓝
                'surprise': (0, 165, 255),  # 橙
            }
            color = color_map.get(label, (0, 255, 0))

            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            # 左上角小圆点
            cv2.circle(frame, (x1, y1), 4, color, -1)

            # 显示表情标签 + 置信度 + emoji
            text = f"{emoji} {label.upper()} {conf:.0%}"
            draw_text_with_bg(frame, text, (x1, y1 - 10),
                             font_scale=0.8, text_color=color, bg_color=(30, 30, 30))

        # 画面提示
        info_bg = (30, 30, 30)
        cv2.rectangle(frame, (0, 0), (350, 25), info_bg, -1)
        cv2.putText(frame, f"Faces: {len(results)}  |  q: quit  s: save",
                    (10, 18), cv2.FONT_HERSHEY_SIMPLEX, 0.55, (200, 200, 200), 1)

        cv2.imshow('Face Emotion Recognition Demo', frame)

        # 按键处理
        key = cv2.waitKey(1) & 0xFF
        if key == ord('q'):
            print("退出")
            break
        elif key == ord('s'):
            save_path = 'emotion_snapshot.jpg'
            cv2.imwrite(save_path, frame)
            print(f"截图已保存: {save_path}")

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
