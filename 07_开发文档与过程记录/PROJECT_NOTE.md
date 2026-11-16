# STM32N6 AI 模型部署 — 开发记录

> **最新状态（2026-08-12）**：IMX335 实时双模型闭环已完成。最新验收数据和根因记录见文末“2026-08-12：IMX335 实时双模型闭环完成”。

## 项目目标
在正点原子 STM32N647 开发板上部署“人脸检测 RFB-320 + 表情识别 ResNet18”双模型系统，使用已接入的 IMX335 MIPI CSI-2 摄像头实时推理并通过 UART 输出结果。OV2640/DCMIPI 保留为可选硬件路线，不是当前主线。

## 核心功能
- STM32N647 基础外设驱动开发
- Neural-ART NPU 加速器使用
- AI 模型量化、转换与部署
- 模型推理结果验证
- 摄像头模块（MIPI CSI-2）接入

## 技术选型
| 层级 | 技术 | 选型理由 |
|------|------|---------|
| IDE | CLion 2025.2 + CubeMX 6.16 | 用户熟悉，F103 开发经验复用 |
| 编译工具链 | ARM GNU GCC 13.3 (STM32CubeCLT 1.20) | E:\ST\STM32CubeCLT_1.20.0 |
| 调试器 | ST-LINK GDB Server (CubeCLT 1.20 自带) | 原生支持 N6 |
| 烧录工具 | STM32CubeProgrammer 2.20 | N6 外部 Flash 烧录必需 |
| HAL 库 | STM32Cube_FW_N6 V1.3.0 | 官方 SDK |
| AI 模型转换 | ST Edge AI Core | N6 NPU 专用（非 X-CUBE-AI！） |

## 阶段规划
1. 阶段 1：环境搭建与验证 ✅ 完成
2. 阶段 2：板级资料获取 ✅ 完成
3. 阶段 3：最小工程编译 ✅ 完成
4. 阶段 4：首次烧录验证（LED 闪烁）✅ 完成
5. 阶段 5：N6 基础外设与 NPU 初始化（GPIO/UART/RIF/RISAF/Cache）✅ 完成当前需求
6. 阶段 6：FP32 模型转换与双模型工程集成 ✅ 完成
7. 阶段 7：固定输入的 FP32 表情模型实机推理 ✅ 完成
8. 阶段 8：静态 QDQ INT8 双模型转换、验证与性能测试 ✅ 完成
9. 阶段 9：IMX335/MIPI CSI-2 摄像头和实时双模型流水线 ✅ 端到端完成
10. 阶段 10：长时间稳定性、场景覆盖、UART 协议和时序平滑 → 当前阶段

## 开发日志
| 日期 | 阶段 | 事件 |
|------|------|------|
| 2026-06-19 | 阶段 1 | 第一轮：环境搭建完成，编译通过，但旧板子烧了 |
| 2026-06-28 | — | **项目重启**：更换新板子，从头开始 |
| 2026-06-28 | 阶段 1~3 | 环境验证通过；获取板级资料；部署 External Loader |
| 2026-06-29 | 阶段 4 | **LED 闪烁成功！** Release 编译 + FSBL + Appli 烧录全链路跑通 |
| 2026-06-29 | 阶段 4 | 理解了 Debug(LRUN) vs Release(ROMxspi2_RAMxspi1) 链接脚本的区别 |
| 2026-06-29 | — | 编写 SOP 文档：`SOP_N6开发流程详解.md` |
| 2026-06-29 | — | 确认 AI 路线：X-CUBE-AI 是给无 NPU 芯片用的，N6 用 ST Edge AI Core |
| 2026-06-29 | — | 编写 CLion 专版 SOP，添加阶段 L（stedgeai AI 模型翻译流程） |
| 2026-06-29 | 阶段 6 | **ST Edge AI Core v4.0.1 安装成功**（`E:\ST\STedgeAi\`） |
| 2026-06-29 | 阶段 6 | **首次 stedgeai generate 成功！** tiny_yolo_v2 模型翻译完成 |
| 2026-06-29 | 阶段 6 | 理解了 stedgeai 用法、mpool 内存池、正点原子配置 vs ST 默认配置 |
| 2026-06-29 | — | 建立学习错题本：`LEARNING_LOG.md`（含 8 个摸底问答 + 4 个开发问题） |
| 2026-06-29 | — | 确立学习优先原则 + 错题本维护规则，存入 memory |
| 2026-07-01 | 阶段 6 | **软件组交付 FP32 opset17 模型**：`best_model_opset17.onnx`（42.6MB, 56 算子, 纯 FP32） |
| 2026-07-01 | 阶段 6 | **stedgeai 翻译成功！** mpool 调整为 octoFlash 20MB + hyperRAM 30MB |
| 2026-07-01 | 阶段 6 | ⚠️ 翻译后 49/51 epoch 跑在 CPU（FP32 残差连接），NPU 利用率极低。去掉 --native-float 无改善 |
| 2026-07-01 | 阶段 4 | **991 例程预编译固件烧录成功**：fsbl.hex + appli.hex + network-data.hex |
| 2026-07-01 | 阶段 5 | **深入分析 991 源码**：提取 NPU 初始化 5 层架构（时钟/RIF/缓存/内存映射/API） |
| 2026-07-02 | 阶段 5 | 用户无 LCD + 无 IMX335，决定换策略：不跑 991，直接搭自己的最小 AI 工程 |
| 2026-07-02 | 阶段 5 | 发现 ResNet18 模型 I/O 接口与 991 不同：固定地址 (0x34200000/0x342e0000) vs User I/O |
| 2026-07-02 | 阶段 5 | 🎓 用户学习：AI 部署宏观概念（y=f(x)）、API 三个函数、内存映射原理、自动生成 vs 手写分工 |
| 2026-07-02 | — | **阶段 A 计划制定**：NPU 裸跑实验 → 静态图推理 → UART 输出 |

## 模型分析（2026-06-29）

> 来源：`E:\STM32N6\02_最终模型文件\best_model_int8.onnx` + README.md（**只读，禁止修改**）

### 基本参数

| 项目 | 值 |
|------|-----|
| 架构 | ResNet18（标准版，无预训练） |
| 格式 | ONNX opset 18，IR v8 |
| 大小 | 11 MB（INT8 量化后） |
| 输入 | `input`，shape `[batch, 3, 224, 224]`（NCHW），RGB |
| 输出 | `output`，shape `[batch, 7]`，**原始 logits**（需端侧 softmax） |
| 归一化 | `x / 127.5 - 1.0`（→ [-1, 1]），等价 mean=0.5, std=0.5 |
| 类别 | Anger(0), Disgust(1), Fear(2), Happiness(3), Neutral(4), Sadness(5), Surprise(6) |
| 精度基线 | FP32: 93.68%（测试集），INT8 量化后待测 |
| 训练 | 自建数据集，已裁 224×224 人脸，未对齐，从头训 80 epoch |
| 量化方式 | ONNX Runtime 动态量化（per-tensor），权重→INT8，激活值→FP32 |
| 人脸检测 | **不包含**——PC 端用 OpenCV Haar Cascade。MCU 端 **不能用 Haar（CPU 跑 ~100ms，成瓶颈）**，必须用 NPU 检测模型（推荐 Ultra-Light-Fast-Generic-Face-Detector-1MB，~300KB） |

### stedgeai 兼容性验证（2026-06-29）

| 测试 | 结果 |
|------|:--:|
| `best_model_int8.onnx` → stedgeai generate | ❌ **失败** |
| 错误信息 | `NOT IMPLEMENTED: Unsupported layer types: MatMulInteger, DynamicQuantizeLinear, ConvInteger` |
| 根因 | ONNX Runtime 动态量化写法（opset18 ConvInteger/DynamicQuantizeLinear），stedgeai v4.0.1 不识别 |
| 对比 | `tiny_yolo_v2_224_int8.tflite`（TFLite 格式）上次成功 ✅ |

**解决方案（二选一）：**
- **方案 A（推荐）**：软件组提供 FP32 原始模型（opset ≤17），stedgeai 自己做量化
- **方案 B**：软件组用 `quantize_static` 重做 QDQ 静态量化，导出 opset 17

### PC 端 Demo 分析（cam_demo.py，2026-06-29）

从软件组的 PC 端代码提取的关键实现细节（MCU 端必须原样复现）：

| 步骤 | 实现 | MCU 端对应 |
|------|------|-----------|
| 人脸检测 | `cv2.CascadeClassifier` + `haarcascade_frontalface_default.xml` | ❌ CPU 太慢，改用 NPU 检测模型 |
| 人脸框扩大 | margin=20%，`detectMultiScale(scaleFactor=1.1, minSize=60)` | 需复现 |
| 颜色转换 | `cv2.COLOR_BGR2RGB` | 摄像头出 YUV→RGB，需软件实现 |
| 缩放 | `cv2.resize(face_roi, (224,224), INTER_CUBIC)` | 需软件实现（可用双线性近似） |
| 归一化 | `(pixel / 127.5) - 1.0` → [-1, 1] | 逐像素乘加 |
| 维度变换 | HWC → CHW，expand batch dim | 可内存重排或传 NCHW 给 NPU |
| 推理 | `ort_session.run(None, {input_name: tensor})` | `LL_ATON_RT_Main()` |
| Softmax | `exp(x - max) / sum(exp(x - max))` | 需软件实现（7 个值，计算量小） |
| 平滑 | deque(maxlen=5)，取最近 5 帧众数 | 可选优化（减少抖动） |

### Haar Cascade vs NPU 检测模型（对比）

| | Haar Cascade（CPU） | Ultra-Light 检测模型（NPU） |
|------|:--:|:--:|
| 运行位置 | Cortex-M55 CPU | NPU |
| 估算耗时 | **50~150ms**（成瓶颈！） | **~1ms** |
| 存储 | ~900KB（XML） | ~300KB（ONNX） |
| 准确率 | 一般（对角度/光照敏感） | 更好 |
| 结论 | ❌ 不可用 | ✅ 必须用 |

### ONNX 内部结构（实测）

| 算子 | 数量 | 数据类型 | 运行位置 |
|------|:--:|------|:--:|
| ConvInteger | 20 | INT8 | NPU ✅ |
| DynamicQuantizeLinear | 18 | 运行时量化 | ⚠️ 待验证 |
| MatMulInteger | 1 | INT8 | NPU ✅ |
| Relu | 17 | — | NPU ✅ |
| MaxPool | 1 | — | NPU ✅ |
| GlobalAveragePool | 1 | — | NPU ✅ |
| Mul | 42 | FP32 | **CPU（残差连接）** ⚠️ |
| Add | 29 | FP32 | **CPU（残差连接）** ⚠️ |
| Cast | 21 | 类型转换 | **CPU** ⚠️ |
| Reshape | 21 | — | CPU |
| 其他 | 8 | — | — |
| **总计** | **178** | | |

### 三个关键风险

| # | 风险 | 说明 | 影响 |
|:--:|------|------|------|
| 1 | **动态量化** | 17 个 `DynamicQuantizeLinear` 节点 → 运行时动态量化激活值，不是静态 QDQ 格式 | stedgeai 兼容性待验证 |
| 2 | **FP32 残差路径** | ResNet18 的 skip connection（42 个 Mul + 29 个 Add）是 FP32，不能跑在 NPU 上 | CPU 处理残差，增加延迟 ~3~5ms |
| 3 | **opset 18** | stedgeai v4.0.1 主要支持 opset 11~17，opset 18 的算子需要实测 | 可能拒绝或 fallback |

### 性能估算（基于软件组确认数据）

| 指标 | 值 |
|------|------|
| 表情模型 MACs | **1.82G**（PyTorch 口径，约 18.2 亿次乘累加） |
| 人脸检测模型 | **Ultra-Light-Fast-Generic-Face-Detector-1MB**，INT8 → ~300KB，MACs ~90M（表情模型的 1/20） |
| NPU 推理（表情，ConvInteger ~1.7G） | ~8.5ms（基于 200 GOPS 有效算力） |
| CPU 残差路径（Mul/Add/Cast） | ~3~5ms |
| **单次表情推理总耗时** | **~12~14ms** |
| NPU 推理（人脸检测） | ~1ms（90M MACs） |
| 预处理（缩放+颜色转换） | ~5~10ms |
| **单帧总耗时估算** | **~18~25ms** |

### 帧率评估（更新）

| 目标 | 每帧预算 | 判断 |
|------|:--:|:--:|
| 15fps | 66ms | ✅ 大幅有余量 |
| 30fps | 33ms | ✅ 可行（18~25ms < 33ms） |

---

## 当前状态

### 2026-07-16：上板调试 — NPU 推理阻塞

#### 🔴 核心问题
stedgeai 翻译生成的模型代码使用了 **Epoch Controller (EC)**，但 stedgeai 编译的 NPU 运行时库 (`libnpu_runtime.a`) 默认是 **Polling 模式**，不支持 EC。导致 `LL_ATON_RT_Main()` 断言失败后直接 hang。

**Polling 模式断言** (`ll_aton_runtime.c:144`):
```c
#if defined(ATON_EPOCHCTRL_NUM) && (LL_ATON_RT_MODE == LL_ATON_RT_ASYNC)
    // 硬件 EC 路径
#else
    LL_ATON_ASSERT(false);  // ← 卡在这里!
#endif
```

#### 🔬 排查过程（7/16，10+ 轮改动）

| # | 尝试 | 结果 |
|---|------|------|
| 1 | 修复 CMake ASM 未启用 → 编译 49行 → 80KB | ✅ 编译通过 |
| 2 | 链接 NetworkRuntime1201_CM55_GCC.a | ✅ |
| 3 | stedgeai `--mode target` 重翻译（纯 ASCII 路径） | ✅ 生成新模型 |
| 4 | 重建 libnpu_runtime.a（target-mode .o 文件） | ✅ |
| 5 | 改 `LL_ATON_PLATFORM` EC_TRACE→STM32N6, RT_MODE POLLING→ASYNC | ❌ 缺 ec_trace 符号 |
| 6 | 添加 CACHEAXI HAL + mcu_cache + npu_cache 编译 | ❌ 仍缺 ec_trace |
| 7 | 添加 ec_trace_unsupported/ec_trace_write stub | ✅ 链接通过 |
| 8 | 用 STM32N6+ASYNC 重编译 ll_aton_runtime.c 覆盖库 | ❌ 缺更多符号 |
| 9 | 添加 IAC 时钟 + NPU Cache 初始化 | ✅ 编译通过 |
| 10 | 验证：ELF 仍包含 `LL_ATON_RT_MODE LL_ATON_RT_POLLING` 字符串 | 🔴 根因确认 |

#### 💡 关键发现

1. **stedgeai `--mode target` 仍编译为 EC_TRACE + POLLING**
   - 生成的 `ll_aton_config.h` 永远写死 `LL_ATON_PLATFORM LL_ATON_PLAT_EC_TRACE` + `LL_ATON_RT_MODE LL_ATON_RT_POLLING`
   - target-mode 只改变优化器和输出格式，不改变平台/RT模式

2. **991 模型 (YOLOv2) 不用 Epoch Controller**
   - 991 的 `network.c` 中没有 `EPOCHCTRL`、`EpochBlock_IsEpochPureSW` 等关键词
   - 所以 POLLING 模式可以正常运行

3. **我们的 ResNet18 用了 Epoch Controller**
   - `user_neuralart.json` 中有 `--enable-epoch-controller` 选项
   - 导致生成的 model 代码包含 EC epoch blocks

#### 📂 新工程目录（7/16）

| 路径 | 用途 |
|------|------|
| `E:\STM32N6\01_双模型主工程\` | 主工程（经过多轮修改） |
| `E:\STM32N6\translate_target\` | stedgeai target-mode 重翻译产物 |
| `E:\STM32N6\expr_991base\` | 基于 991 框架的新尝试（未完成） |

#### 🔄 下一步（7/17）

- [ ] **方案 A（推荐）**: 去掉 `--enable-epoch-controller`，重新 stedgeai 翻译
  - 修改 `user_neuralart.json`，移除该选项
  - 用 target mode 重翻译，生成无 EC 的模型代码
  - 这样可以和 991 模型一样在 POLLING 模式下工作
  
- [ ] **方案 B**: 如果方案 A 的模型精度/性能下降，需要完整解决 ASYNC 模式
  - 需要找到真正为 STM32N6 硬件编译的 NPU 运行时库
  - 或者从 991 CubeIDE 工程提取编译参数

- [ ] 解决后：上板 UART 验证推理输出

#### 📊 烧录验证状态

| 文件 | 状态 |
|------|:--:|
| fsbl.hex | ✅ 已烧录 (0x70000000) |
| network-data.hex | ✅ 已烧录 (0x70200000 + 0x71600000) |
| appli.hex | ✅ 可正常烧录 |

系统启动正常，UART 输出正常，CPU 800MHz 正确识别。仅 NPU 推理阻塞。

---

## 板子操作备忘录
| 操作 | BOOT0 | BOOT1 | 工具 |
|------|-------|-------|------|
| 烧录 | GND | 3.3V | CubeProgrammer (ST-LINK) |
| 运行 | GND | GND | 上电即跑 |
| 接线 | USB UART(供电) + ST-LINK(烧录) 两根都要插 | | |

---

## 2026-07-17：NPU 利用率分析 + 去 EC 重翻译 + 编译成功

### 模型 NPU 占比分析

stedgeai 翻译报告分析结果：

| 运行位置 | Epoch 数 | 占比 |
|----------|:--:|:--:|
| CPU (SW) | 49/51 | 96.1% |
| Hybrid | 1/51 | 2.0% |
| NPU (HW) | 1/51 | 2.0% |

**根因：模型是 FP32，NPU 只加速 INT8。** 21 层 Conv 全部是 `Conv(float)`，1.82G MACs 几乎全跑在 CPU。

**更正（2026-07-20）**：`--fake-Qmn` 只在软件路径模拟定点量化，实测仍为 49/51 个纯软件 epoch，不能把 FP32 Conv 变成 NPU 可执行的 INT8 Conv。正确路线是使用代表性校准数据生成静态 QDQ INT8 ONNX，或使用 QAT 后导出 QDQ 模型。

### 去 EC 重翻译 ✅

- 创建 `E:\STM32N6\no_ec_test\` 工作目录
- 修改 `user_neuralart.json`，移除 `--enable-epoch-controller`
- stedgeai generate 成功
- 验证结果：
  - `network.c` 中 EC 关键词：**0 处**（旧版有 1 处）
  - `network_ecblobs.h`：**不再生成**
  - epoch_1 类型：EC → **HW**
  - 权重 .raw 文件 MD5 与旧版相同（不需重烧 network-data）

### 工程更新 ✅

1. 替换 `Appli/Models/expr_net/` 为新模型文件（备份旧文件为 .bak_ec）
2. 删除残留 `network_ecblobs.h` 和 `network_blob_trace.c`
3. 用新 .o 文件重建 `libnpu_runtime.a`（16 个 .o，不含模板文件）
4. **CMakeLists.txt 修复**：
   - 移除 `LL_ATON_RT_MODE=LL_ATON_RT_ASYNC` 编译定义
   - 移除 `ll_aton_runtime.c` 源码覆盖（恢复使用库版本）
5. **ll_aton_config.h 修复**：`LL_ATON_RT_ASYNC` → `LL_ATON_RT_POLLING`

### Release 编译 ✅

- 15/15 全过，仅 1 个无害 warning（USE_HAL_DRIVER 重定义）
- ELF: 1.5MB, ROM: 81KB, RAM: 607KB
- HEX 已生成：`E:\STM32N6\01_双模型主工程\appli.hex`（228KB）

### 🔴 烧录阻塞：ST-LINK 无法连接 MCU

- ST-LINK SN: E1007200D0D2139393740544, 电压 3.22V 正常
- CubeProgrammer v2.22.0 / v2.21.0 均报 "Unable to get core ID"
- GDB Server 报 "Unknown MCU found on target"（能检测到但无法识别）
- 尝试过：降低 SWD 频率、UR/HotPlug/Normal 模式、hardRst、shared 模式
- CubeCLT 自带 CubeProgrammer（v2.21.0）也不行
- **待重启电脑后再试**

### 下一步

- [ ] 重启后重新尝试 ST-LINK 连接烧录
- [ ] 烧录成功后 UART 验证推理（LED 闪烁 + printf 输出）
- [ ] 确认去 EC 后 Polling 模式工作正常
- [x] `--fake-Qmn` 路径已验证并否决：不会改变 FP32 Conv 的 CPU/NPU 分配；改走静态 QDQ INT8/QAT。

---

## 2026-07-19 ~ 2026-07-20：阶段 1 模型上板推理完成

### 实机里程碑

- ResNet18 表情模型在固定全零输入上完成一次端到端推理。
- UART 依次输出网络初始化、输入拷贝、Cache Clean、推理完成、输出拷贝和 7 类概率。
- 第一阶段“摄像头接入前，证明模型可在 STM32N647 上运行”完成。
- 当前应用 HEX SHA-256：`531AA27C3D653102F2C680D0574906D93DE5FC1B072C0E0A2F4F90137E6CC9A9`。

### 问题、原因与解决方式

| 问题 | 根因 | 解决方式 | 验证证据 |
|------|------|----------|----------|
| NPU 首个 HW epoch 永久等待 | 应用只调用 `stai_network_init()`，遗漏全局 `stai_runtime_init()`，ATON Stream Switch 未初始化 | 在网络实例初始化前调用并检查 `stai_runtime_init()` | STRENG6 原先仅读 0x180 字节并报 BUFBL；修复后 `stai_network_run()` 正常返回 |
| 初期 BusFault/看门狗行为异常 | 运行时曾包含 SWEMUL 路径，工程本地 `ATON.h` 还覆盖硬件寄存器读取 | 为 STM32N6 目标重建 ATON runtime，移除错误寄存器覆盖 | ELF 反汇编访问真实 NPU 寄存器区，不再访问 0x15000 模拟器地址 |
| 30 秒超时判断不可靠 | 早期看门狗按循环次数而非毫秒计时 | 改为基于 `HAL_GetTick()` 的无符号时间差 | 固件校验脚本加入回归检查，实机能稳定产生 30,000 ms 诊断 |
| UART 百分比字段为空 | 使用 `--specs=nano.specs`，但未链接 newlib-nano 的浮点格式化器 | 链接选项加入 `-u _printf_float` | ELF 包含 `_printf_float`，用户确认 7 类百分比显示正常 |
| `--fake-Qmn` 后仍很慢 | 它不是带校准参数的真实模型量化，Conv 仍是 FLOAT32 | 放弃该路径，要求静态 QDQ INT8 或 QAT 模型 | 翻译报告仍为 49/51 个纯软件 epoch |

### 当前模型状态

| 模型 | 精度 | 计算量 | ST Edge AI 分配 | 结论 |
|------|------|--------|----------------|------|
| 表情 ResNet18 | FP32 | 1,818,461,895 MAC | 49 SW + 1 Hybrid + 1 HW | 已跑通，但不满足实时要求 |
| 人脸 RFB-320 | FP32 | 103,819,604 MAC | 100 SW + 6 Hybrid + 2 HW | 尚未在当前应用中启用，必须量化/拆分后处理 |

### 下一阶段目标

1. 软件组交付静态 QDQ INT8 表情模型、量化脚本、校准说明和精度报告。
2. 用 ST Edge AI 4.0.1 验证模型：主干不得再出现大量 `Conv(float)`，主要 MAC 的 NPU 覆盖率目标不低于 90%。
3. 替换模型并重新生成 network-data，测量实际板端延迟和精度，目标先达到稳定 15 FPS 所需的模型预算。
4. 对人脸检测模型执行同样的 INT8 改造；优先把框解码、Softmax、Exp、NMS 移到 MCU 后处理。
5. 在无摄像头条件下依次跑通“人脸模型固定输入 → 裁剪/缩放测试数据 → 表情模型固定输入”的双模型链路。
6. 双模型门槛通过后，再接入 OV2640/DCMIPI、帧缓冲、颜色转换和实时循环。
# 2026-08-12：IMX335 实时双模型闭环完成

## 实机验收结果

- IMX335 连续采集正常，内部 test pattern 与真实感光画面均验证。
- PIPE2 输出 RGB888 320x240，HWC->CHW INT8 预处理正常。
- RFB-320：NPU 约 12 ms，人脸最高置信度 0.996。
- ResNet18：推理约 15 ms，本次结果 Happiness 42.8%。
- 有脸帧端到端约 84 ms。
- AE 已能通过 PIPE2 主帧统计自动调整曝光和增益。

## 最终根因

1. IMX335 初始曝光使用 `exposure_min=0 us`，造成 `SHS1=VMAX`、有效积分为零，真实画面全黑。
2. 原 ISP 中间件假设 PIPE1 是主流；本工程只启动 PIPE2，PIPE2 VSYNC 未调用 `ISP_IncMainFrameId()` / `ISP_GatherStatistics()`，导致 AE/AWB 无统计输入。
3. 早期 DCMIPP 时钟也配置错误；恢复正点原子 800 MHz 参考配置 `IC17=PLL2/3`、`IC18=PLL1/40` 后，PIPE2 overrun 消失。

## 已实施修复

- streaming 前设置 `IMX335_EXPOSURE_DEFAULT=23814 us` 和 `IMX335_GAIN_DEFAULT=20000 mdB`。
- PIPE2-only 模式下，将 PIPE2 VSYNC 接入 ISP 主帧统计路径。
- 使用 continuous capture，预热 30 帧运行 AE/AWB。
- CPU 读取帧时 suspend PIPE2，cache invalidate + HWC->CHW 后 resume，避免 DMA/CPU 同时访问产生撕裂。
- 保留首帧数据、曝光和硬件错误诊断，便于后续稳定性测试。

## 验证数据

```text
VMAX=4500 SHS1=9 exposure_lines=4491 GAIN=36 (10.8 dB)
camera RGB: min=0 max=131 avg=70
face score=0.996, box=(0.018,0.000)-(0.699,0.529)
expression=Happiness 42.8%
total=84 ms
```

## 下一阶段

进入稳定性和接口阶段：长时间运行、环境覆盖测试、UART 返回值协议、置信度/多帧平滑，以及按需要升级为双缓冲采集。

## 2026-08-12：完整问题复盘索引

本节是开发记录的结论索引；完整现象、证据、失败尝试和修复说明统一维护在 `E:\STM32N6\HANDOFF.md` 第 4 节，避免后续只看到某次中间实验而误认为是当前状态。

| 类别 | 已解决问题 | 最终原因 | 最终处理 |
|---|---|---|---|
| 模型格式 | 动态 INT8 ONNX 无法由 ST Edge AI 4.0.1 翻译 | `ConvInteger`/`DynamicQuantizeLinear` 等图结构不受支持 | 改为静态 QDQ INT8/QAT 交付 |
| NPU 性能 | FP32 表情模型绝大多数 epoch 跑 CPU | FP32 Conv 无法形成 N6 NPU 的主要 INT8 加速路径 | 两个模型均改用 QDQ INT8 |
| NPU 启动 | 首个 HW epoch 在 `LL_Streng_Wait` 超时 | 遗漏全局 `stai_runtime_init()`，Stream Switch 未初始化 | runtime init 必须先于任一 network init/run |
| 运行时 | SWEMUL/EC/POLLING 混用引发异常或误判 | 生成物和 runtime 配置不一致，且遮蔽了 runtime init 缺失 | 使用目标 runtime 并以实机寄存器/调用路径验证 |
| 内存 | 人脸 FP32 激活内存过大 | 约 6 MB 激活超出片上 SRAM | INT8 后约 640 KB，放入片上 SRAM |
| 输出 | UART `%f` 为空 | newlib-nano 未链接浮点 printf | 加 `-u _printf_float` |
| 栈 | NMS 后 HardFault | 约 106 KB 局部数组炸栈 | 改为有界静态缓冲区 |
| 输入 | 固定图片成功、相机始终 0 人脸 | HWC RGB888 被当作 CHW int8，且遗漏量化 | HWC->CHW 并按 scale/zp 量化 |
| I2C | IMX335 `ID=FFFFFFFF` | 传感器未应答，属于供电/复位/引脚/MSP 初始化链路 | 对照板级参考工程恢复 I2C2 和传感器初始化 |
| DCMIPP | PIPE2 timeout/overrun/帧不完整 | DCMIPP 和 CSI 内核时钟配置错误 | `IC17=PLL2/3`、`IC18=PLL1/40` |
| 感光 | 数据链可用但真实画面全黑 | 初始曝光 0 us 导致 `SHS1=VMAX`、有效积分为 0 | streaming 前设置 23814 us 基线曝光 |
| 亮度 | 恢复曝光后画面仍暗 | 初始增益不足，AE 尚未闭环 | 设置 20 dB 基线增益并修复 ISP 统计 |
| AE/AWB | PIPE2-only 时曝光增益不自动变化 | 中间件只把 PIPE1 当主帧，PIPE2 没有提交统计 | PIPE2 VSYNC 接入 `ISP_IncMainFrameId`/`ISP_GatherStatistics` |
| 采集状态机 | snapshot 后永久等待下一帧 | 单帧模式不匹配实时循环 | continuous + 30 帧预热 + suspend/read/resume |
| Cache/DMA | 单缓冲有覆盖和撕裂风险 | DMA 写与 CPU 读可能并发 | 完整帧时 suspend，invalidate/转换后 resume |

最终验收基线：RFB-320 NPU 约 12 ms、ResNet18 约 15 ms、真人脸 score 0.996、有脸帧总耗时约 84 ms。应用 HEX SHA256 为 `1A54CFF46EE1764FE4DB28485BE34749CDB4D1CE9FC1947B8C2D0690FBE8BFCE`。

---

## 2026-08-13：首帧成功后的第二轮 I2C HardFault

### 当前结论

- 首帧相机、人脸和表情链路仍正常，真人脸 score 0.996，总耗时约 84 ms。
- 首帧输出完成后，第二轮 `app_camera_isp_update()` 稳定 HardFault。
- `PC=0x70107ACC` 映射到 `I2C_WaitOnFlagUntilTimeout`；`R0=0x340B6454` 是 `hi2c2`；`BFAR=0x18` 证明 `hi2c2.Instance` 已变为 NULL。
- 删除表情路径中重复的 `stai_runtime_init()` 后，故障现场没有变化。该生命周期清理保留，但此假设已被实机否定为直接根因。

### 本轮保存内容

- `main.c`：共享 STAI runtime 只初始化一次。
- `stm32n6xx_it.c`：保留应用阶段号、原始异常 SP、basic/extended PC/LR 和原始堆栈诊断。
- `tools/test_stai_runtime_single_init.ps1`：约束全局 runtime 只能调用一次，且 `run_expression()` 不得再次初始化。
- 当前应用 HEX SHA256：`7035281784DBE1FF4A67971A4E6B7B97F699258F42E7923A1FDD30DC46DA312D`，已成功烧录并 verify。

### 下次第一步

不要直接恢复 I2C 句柄。先在首帧各 AI/后处理边界检查 `hi2c2.Instance`，定位首次写坏的阶段；必要时对 `0x340B6454` 设置硬件写观察点，捕获真正的写入者。完整排查顺序见 `E:\STM32N6\HANDOFF.md` 第 9 节。

本轮已编译诊断固件：在各 AI 边界及每次 ISP 更新前检查 `hi2c2.Instance`，不重设句柄；检测到破坏时输出 `[CORRUPTION]` 后停机。HEX SHA256 为 `D12D3AB563788060ACFBCA0CB0CB7D1C822C5559E6369494763C56014AE671B1`，等待实机验证。

---
