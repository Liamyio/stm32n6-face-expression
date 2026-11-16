# 需求讨论记录 — STM32N6 AI 模型部署

## 讨论日期
2026-06-19 ~ 2026-06-20

## 关键决策

### Q1: IDE 选择 — CLion+CubeMX vs CubeIDE
**决策:** 继续使用 CLion+CubeMX
**理由:** 用户 F103 开发经验在此，且 CubeMX 6.16 + CubeCLT 1.20 完全支持 N6。CubeMX 在 N6 上的角色从"可选辅助"变为"必须依赖"——N6 的 XSPI 外部内存和 NPU 时钟手写代码不现实。

### Q2: 调试服务器 — OpenOCD vs ST-LINK GDB Server
**决策:** 使用 ST-LINK GDB Server（CubeCLT 1.20 自带）
**理由:** OpenOCD 0.12（2023）早于 N6 发布（2024），不支持。ST-LINK GDB Server 源自 ST 官方，对新芯片支持完善。

### Q3: 工程结构 — 双层 CMake vs 单层
**决策:** 接受 N6 的双层工程结构（根 CMake + Appli 子项目）
**理由:** N6 的 TrustZone 安全模式需要 Boot + Appli 分离架构。根 CMakeLists.txt 是调度器，Appli/ 是用户代码区。

### Q4: 链接脚本 — 8 个 .ld 文件的理解
**决策:** 理解命名规则，Debug 用 LRUN（内部 RAM），Release 用 ROMxspi2_RAMxspi1（外部 Flash + HyperRAM）
**理由:** N6 片上存储太小（64KB Flash），大程序必须放外部。链接脚本 = 内存地图，必须和硬件实际接线匹配。

### Q5: CLion 配置切换 — Debug/Release 自动选链接脚本
**决策:** 通过 CMake if/elseif 根据 CMAKE_BUILD_TYPE 自动选择，而非每次手动改文件
**理由:** 对应 CubeIDE 的 Debug/Release 配置概念，但用代码而非 GUI 实现。

### Q6: 烧录方案 — 独立 CubeProgrammer
**决策:** CLion 编译 + CubeProgrammer 烧录的两步工作流
**理由:** N6 外部 NOR Flash 需要 CubeProgrammer 的专用初始化时序和擦除算法，CLion 内嵌调试无法处理。

## 2026-06-28 项目重启
旧板子烧毁，换新板重新开始。工具链环境保留，工程重新生成。等待用户布置新任务。

## 2026-06-29 目标对齐与模型分析
- 最终目标明确：摄像头（MIPI CSI-2）→ STM32N647 → 10MB 表情识别模型 → 实时推理
- 目标帧率：先以 15fps 为目标，争取 30fps
- 软件组交付了表情识别模型：`Final AI/best_model_int8.onnx`（ResNet18，11MB，INT8 动态量化）
- ONNX 内部结构实测：178 个算子，含 18 个 DynamicQuantizeLinear + 42 个 FP32 Mul + 29 个 FP32 Add
- 三个关键风险确认：
  1. 动态量化 vs NPU 兼容性（stedgeai 待实测）
  2. FP32 残差路径 fallback 到 CPU（增加延迟）
  3. opset 18 兼容性
- 人脸检测缺口：模型不含检测，MCU 端需要独立方案（待软件组确认）
- 待问软件组：MACs 精确值、量化后精度、MCU 端人脸检测方案

## 2026-06-29 模型兼容性验证 + PC Demo 分析
- **stedgeai 试译失败**：`best_model_int8.onnx` → `NOT IMPLEMENTED: MatMulInteger, DynamicQuantizeLinear, ConvInteger`
- 根因：ONNX Runtime 动态量化 + opset 18 不兼容。需软件组提供 FP32（opset ≤17）或 QDQ 静态量化版本
- **PC Demo 分析**（`cam_demo.py`）：
  - 人脸检测：Haar Cascade（PC 没问题，MCU 不行）
  - 预处理：BGR→RGB + resize 224×224 + x/127.5-1.0 + HWC→CHW
  - 后处理：softmax（exp(x-max)/sum） + argmax + 5 帧平滑
  - 人脸框扩大 20%
- **Haar Cascade 结论**：MCU 端不可用（CPU ~100ms/帧，成绝对瓶颈），必须用 NPU 检测模型
- 软件组待交付：① 表情 FP32（opset ≤17） ② 人脸检测 ONNX（Ultra-Light ~300KB）

## 2026-06-29 人脸检测模型获取 + 翻译成功
- 从开源下载 `version-RFB-320.onnx`（opset 9, FP32, 242 算子, 1.2MB）
- opset 9 直接失败（Slice 缺 orig_steps）→ onnxsim 简化 → 升级 opset 11 → **stedgeai 翻译成功！**
- 关键数据：MACs=104M, 权重=1.14MB（NOR Flash 0x70200000），激活=4.8MB（npuRAM+HyperRAM）
- 解决方案路径确认：onnxsim simplify → version_converter upgrade opset → stedgeai pass
- 确立学习优先策略：先跑正点原子 AI 例程，再部署自己的模型

## 2026-07-01 ~ 2026-07-02 模型翻译 + 策略调整

### 软件组交付 FP32 opset17 模型
- 文件：`best_model_opset17.onnx`（42.6MB，56 算子，纯 FP32）
- 与旧模型对比：178 算子 → 56 算子，DynamicQuantizeLinear 全部消失
- stedgeai 翻译成功，但需调整 mpool：octoFlash 11→20MB，hyperRAM 16→30MB

### stedgeai 翻译结果分析
- 翻译 PASS（生成 10 个文件）
- ⚠️ 49/51 epoch 跑在 CPU（FP32 残差连接），NPU 仅加速 1 个 epoch
- 去掉 `--native-float` 无改善——这是 ResNet18 架构级限制
- 先接受现状，上板实测后再决定是否优化

### 991 例程烧录 + 策略调整
- 预编译固件烧录成功（fsbl + appli + network-data 三合一）
- 用户硬件限制：无 LCD 屏幕、无 IMX335 摄像头（有 OV2640）
- **策略转变**：不编译 991 原版（依赖太多且看不到输出），改从 991 学 API 后直接搭自己的工程

### NPU 最小工程路线（阶段 A）
- 从 991 提取 NPU 初始化 5 层架构：时钟 → RIF → Cache → 内存映射 → API
- 从 LED 工程复制模板，加入模型文件 + NPU Runtime
- 用静态图片跑推理，UART 打印结果
- 发现 ResNet18 模型 I/O 接口与 991 不同：固定地址 vs User I/O

### 新增理解
- AI 部署本质：y = f(x)，输入像素 → 输出数字 → 取最大 → 打印标签
- API 三个来源：ST 头文件（宏）+ stedgeai 生成（Set_User）+ ST 闭源库（RT_Main）
- 内存映射：XSPI 控制器把 SPI 协议隐藏，NPU 以为在访问普通内存
- `#ifdef DEBUG` 块的作用：Release 时 FSBL 已初始化 XSPI，Appli 不需要重复

## 2026-07-07 ~ 2026-07-08：最终项目工程搭建

### 创建「表情识别」工程
- 从 001_LED 模板复制工程框架
- 工程路径：`E:\STM32N6\01_双模型主工程\`
- 目标：表情识别 + 人脸检测双模型，UART 输出，无需屏幕

### 双模型符号冲突解决
- **问题**：stedgeai 为每个模型生成相同的函数名（`stai_network_init`, `LL_ATON_NETWORK_*` 等）
- **根因**：stedgeai 使用 `C_MODEL_NAME = "network"` 作为默认名，两个模型都用这个名字
- **解决方案**：对人脸检测模型做全局符号重命名
  - `LL_ATON_NETWORK_*` → `LL_ATON_FACETNET_*`
  - `stai_network_*` → `stai_facenet_*`
  - 头文件改名：`network.h` → `facenet_network.h`, `stai_network.h` → `facenet_stai.h`
  - 注意保留通用类型 `stai_network`（实际上是 `typedef uint8_t`）不变
- **知识点**：
  - `stai_network` 是 ST 的通用 opaque handle（本质是 `uint8_t`），不是在 model 代码中定义的
  - 静态函数（`static void LL_ATON_End_EpochBlock_*`）不会导致链接冲突
  - 非静态函数和宏才会冲突
  - 两个模型的 `#define STAI_NETWORK_IN_NUM` 值不同（表情=150528 元素，人脸=230400 元素）

### xSPI2 NOR Flash 地址冲突解决
- **问题**：两个模型的 stedgeai 翻译都默认 xSPI2 起始地址 = 0x70200000
- **分析**：
  - 表情模型 xSPI2.raw = 20MB（bias 数据），xSPI1.raw = 30MB（weights）
  - 人脸检测 xSPI2.raw = 1.2MB（weights），xSPI1 仅运行时激活（无 raw 文件）
  - 总需求 = 20MB + 1.2MB = 21.2MB < 32MB NOR Flash
- **解决方案**：重翻译人脸检测模型，xSPI2 从 0x70200000 → **0x71600000**
  - 修改 mpool 中 xSPI2 offset 从 1794 MB → 1814 MB
  - stedgeai 使用纯 ASCII 路径（at-onn.exe 不支持中文路径！）
  - 只需重翻译较小的模型（1.2MB vs 20MB）
- **知识点**：
  - mpool 是给 stedgeai 的内存地图，决定权重放哪个物理地址
  - `.raw` 文件 = 要烧录到该地址的二进制数据
  - `network.c` 中硬编码了 `0x71600000UL + offset` 的地址引用
  - 改地址 = 改 mpool → 重新 stedgeai generate → 得到新 network.c + new .raw

### NPU 运行时库构建
- **来源**：stedgeai 翻译时生成的 `st_ai_ws/build_rt_network/*.o` 文件
- **关键文件**（16 个 .o）：
  - `ll_aton_rt_main.o` — `LL_ATON_RT_Main()` 主入口
  - `ll_aton.o`, `ll_aton_runtime.o` — NPU 运行时
  - `ll_aton_lib.o`, `ll_aton_lib_sw_operators.o` — NPU + CPU 算子库
  - `ll_sw_float.o`, `ll_sw_integer.o` — CPU fallback 算子
  - `ecloader.o` — Epoch Controller 加载器
  - `stedgeai_hsp_stubs.o` — 硬件平台桩函数
- **注意**：不能包含模板的 `network.o`/`stai_network.o`（那是示例模型的，会和我们的冲突）
- 用 `arm-none-eabi-ar rcs libnpu_runtime.a *.o` 打包

### NPU 初始化 5 步法
基于 ST NPU_Validation 示例提取：
1. **使能 NPU SRAM** — `RCC->MEMENR |= AXISRAM3~6EN + CACHEAXIRAMEN`
2. **清除 deep-sleep** — `RAMCFG_SRAMx_AXI->CR &= ~RAMCFG_CR_SRAMSD`
3. **允许 Cache 激活** — `MEMSYSCTL->MSCR |= DCACTIVE | ICACTIVE`
4. **NPU 时钟** — `__HAL_RCC_NPU_CLK_ENABLE()` + reset 序列
5. **RIF 安全配置** — `HAL_RIF_RIMC_ConfigMasterAttributes(NPU, SEC|PRIV)`

### 编译问题解决记录
| 问题 | 原因 | 解决 |
|------|------|------|
| `AI_ALIGNED` 未定义 | 需要 `ai_platform.h` | 添加 `#include "ai_platform.h"` |
| `GPIO_AF6_USART1` 不存在 | N6 的 USART1 用 AF7 | 改为 `GPIO_AF7_USART1` |
| `UART_InitTypeDef.FifoMode` 不存在 | N6 HAL 无此字段 | 改为 `ClockPrescaler` |
| CMake plain/keyword 签名冲突 | mx-generated 用 plain | 统一为 plain 签名 |
| stedgeai `wstring_convert::from_bytes` | 中文路径 | 用纯 ASCII 路径运行 |
| `__WFE`/`NVIC_EnableIRQ` 重定义警告 | NPU 平台头 vs CMSIS | 无害警告，NPU 有意覆盖为空宏 |

## 2026-07-20：阶段 1 完成与下一阶段模型决策

### 已确认结果

- 固定输入的 FP32 ResNet18 已在 STM32N647 上完成一次完整推理，UART 输出 7 类概率。
- 首 epoch 卡死最终由遗漏 `stai_runtime_init()` 导致；补充全局运行时初始化后解决。
- UART 浮点字段为空由 newlib-nano 未链接 `_printf_float` 导致，现已解决。

### 模型现状决策

- 当前表情 ResNet18 是 FP32：1.818 GMAC，49/51 个纯软件 epoch。
- 当前人脸 RFB-320 也是 FP32：103.8 MMAC，100/108 个纯软件 epoch。
- 旧动态量化 ONNX 和 `--fake-Qmn` 不再作为加速方案。
- 下一版模型必须是使用代表性数据校准的静态 QDQ INT8；首选 opset 17、固定 batch=1、固定输入 shape。
- 表情接口保持 RGB/NCHW `[1,3,224,224]`，预处理 `x/127.5-1`，输出 `[1,7]` logits，类别顺序不变。
- 精度验收目标：INT8 相对 FP32 Top-1 下降不超过 1 个百分点，最多不超过 2 个百分点，并提供逐类指标和混淆矩阵。
- 部署验收目标：主干不能大量 fallback 为 `Conv(float)`，主要 MAC 的 NPU 覆盖率目标不低于 90%。

### 下阶段执行顺序

1. 接收并检查表情 QDQ INT8 ONNX、量化脚本、校准集说明、精度报告和测试向量。
2. stedgeai 翻译并检查 epoch/内存报告，重新分配 NOR Flash 与 HyperRAM。
3. 固定输入上板，对比软件组测试向量并测量真实推理延迟。
4. 量化或重构 RFB-320；将不适合 NPU 的检测框解码和 NMS 留在 MCU。
5. 无摄像头条件下跑通双模型数据流，再开始 OV2640/DCMIPI 集成。
