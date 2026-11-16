# STM32N6 双模型 AI 项目交接文档

更新时间：2026-08-13

## 1. 项目目标

在正点原子 STM32N647 开发板上部署 IMX335 摄像头、人脸检测 RFB-320 和七类表情识别 ResNet18，通过 USART1 输出实时结果。

## 2. 硬件与工具链

- STM32N647，800 MHz，32 MB NOR Flash，32 MB HyperRAM
- 正点原子 IMX335 MIPI 摄像头
- ST-LINK/V3，USB-UART CH340，USART1 115200-8-N-1
- STM32Cube FW N6 1.3.0，STM32CubeCLT GCC 13.3，CMake/Ninja
- ST Edge AI Core 4.0.1，STM32CubeProgrammer 2.20

## 3. 当前状态

双模型摄像头链路已经跑通：连续采集、HWC RGB888 转 CHW INT8、人脸检测、NMS、人脸裁剪和表情识别均已验证。实机连续多帧检测到人脸并输出表情，最近测试未出现 HardFault 或 context corruption。

实测性能：人脸约 12 ms，表情约 37 ms，整帧约 101 ms，约 10 FPS。

## 4. 运行链路

```text
IMX335 RAW10 -> MIPI CSI-2 -> DCMIPP PIPE2 RGB888 320x240
-> HWC 转 CHW + INT8 量化 -> RFB-320 -> 解码/NMS
-> 最高置信度人脸裁剪 -> ResNet18 -> Softmax -> USART1
```

## 5. 模型接口

### 人脸检测

- RFB-320，INT8
- 输入：3x240x320，CHW，scale 0.0078125，zero point -1
- 输出：4420 anchors，score 阈值 0.50，NMS IoU 0.45
- ST Edge AI：52 HW、12 Hybrid、1 SW epoch

### 表情识别

- ResNet18，静态 QDQ INT8
- 输入：3x224x224，CHW
- 输出：7 个 int8 logits
- 54/54 HW epochs

## 6. 开发问题、原因与解决方案

1. FP32 推理很慢：模型包含大量软件 epoch，NPU 利用率低。软件组改为静态 QDQ INT8，表情模型达到 54/54 HW。
2. 摄像头检测不到人脸：DCMIPP 输出 HWC RGB888，模型需要 CHW INT8。增加通道重排、归一化和量化查找表后恢复检测。
3. PIPE2 frame timeout：连续采集与 DMA 缓存同步不完整。采用帧序号等待、PIPE2 suspend、D-Cache invalidate、读取、resume。
4. 画面全黑或亮度异常：IMX335 AE/AWB 尚未稳定。启动后等待 30 帧，并保留曝光/增益初始化。
5. 表情推理后 I2C HardFault：ATON runtime 的执行状态写越生成 API 公布的 context 大小，覆盖相邻 `hi2c2`。当前方案为表情 context 后增加 256 字节 runtime slack，并在每帧推理结束后清空 context、下一帧重新初始化。该方案已实机验证稳定，清理代码时必须保留。
6. 早期 NPU timeout：未正确调用全局 `stai_runtime_init()` 或使用了不匹配的旧 runtime 头文件。当前启动顺序先初始化 NPU、RIF、cache，再调用全局 runtime 初始化。

## 7. 当前源码约定

- 正常串口只输出初始化、帧状态、检测结果、表情结果、耗时和必要错误。
- 详细寄存器转储、首帧数据统计、context guard 等诊断代码已移除。
- `g_fault_stage` 保留给 HardFault 处理器记录异常现场，不参与正常业务输出。
- 静态 NMS 缓冲不能改为栈变量，否则会造成栈溢出。

## 8. 编译与烧录

```powershell
cd E:\STM32N6\01_双模型主工程
cmake --build Appli\build --parallel 4
```

烧录模式：BOOT0=GND、BOOT1=3.3V。烧录应用 hex、FSBL 和两个模型数据文件后，将 BOOT1 置 GND 运行。

## 9. 验证清单

- 编译无错误：`cmake --build Appli/build --parallel 4`
- 检查诊断残留：
  `rg "CAM-DIAG|NPU-DIAG|EXPR-DIAG|CORRUPTION|CAM-SENSOR" 01_双模型主工程/Appli/Core/Src`
- 上电后确认初始化成功、连续采集启动、检测到人脸、输出表情。
- 至少连续运行 30 分钟，观察是否出现 HardFault、I2C 错误或 PIPE2 timeout。

## 10. 后续工作

1. 进行长时间稳定性测试和多种光照/距离测试。
2. 评估多脸逐一表情识别和更高帧率方案。
3. 若继续升级 ST Edge AI runtime，必须重新验证 context 大小和相邻外设句柄完整性。
