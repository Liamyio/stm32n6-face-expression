# SOP: STM32N6 双模型 AI 工程搭建全流程

> **目标**: 在正点原子 STM32N647 开发板上部署「人脸检测 + 表情识别」双模型 AI 系统
> **日期**: 2026-07-07 ~ 2026-07-08
> **状态**: 工程编译通过，待上板验证

---

## 目录

1. [前置准备：环境与工具](#1-前置准备环境与工具)
2. [模型翻译：stedgeai generate](#2-模型翻译stedgeai-generate)
3. [双模型共存：符号重命名](#3-双模型共存符号重命名)
4. [内存规划：地址冲突解决](#4-内存规划地址冲突解决)
5. [工程搭建：从 LED 模板到 AI 工程](#5-工程搭建从-led-模板到-ai-工程)
6. [NPU 运行时：静态库构建](#6-npu-运行时静态库构建)
7. [NPU 初始化：时钟、RIF、Cache](#7-npu-初始化时钟rifcache)
8. [应用代码：main.c 编写](#8-应用代码mainc-编写)
9. [构建系统：CMake 配置](#9-构建系统cmake-配置)
10. [烧录与测试策略](#10-烧录与测试策略)
11. [知识点索引](#11-知识点索引)

---

## 1. 前置准备：环境与工具

### 1.1 工具链

| 工具 | 版本/路径 | 用途 |
|------|----------|------|
| ARM GCC | 13.3.1 (`E:\ST\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\`) | 交叉编译 |
| CMake + Ninja | CubeCLT 自带 | 构建系统 |
| STM32CubeProgrammer | 2.20 | 烧录（外部 Flash 需要 External Loader） |
| ST Edge AI Core | 4.0.1 (`E:\ST\STedgeAi\`) | ONNX→NPU 模型翻译 |
| CubeMX | 6.16 | .ioc 工程生成（仅用于初始模板） |
| STM32Cube FW N6 | V1.3.0 (`C:\Users\liam\STM32Cube\Repository\STM32Cube_FW_N6_V1.3.0\`) | HAL 驱动 |

### 1.2 硬件

| 硬件 | 说明 |
|------|------|
| 正点原子 STM32N647 开发板 | N6 芯片，32MB NOR Flash + 32MB HyperRAM |
| ST-LINK/V3 | 烧录调试 |
| USB-UART (CH340) | 串口输出（PA9/PA10 或 PE5/PE6） |
| OV2640 摄像头 | DCMI 并口（暂未接入，测试阶段用静态图片） |

> **知识点：为什么用 CubeProgrammer 而不是 OpenOCD？**
> N6 的外部 NOR Flash 烧录需要通过 XSPI 接口发送 Octal-SPI 命令，这需要 CubeProgrammer 的 External Loader（.stldr 文件）来初始化 XSPI 时序和 Flash 芯片。OpenOCD 0.12 早于 N6 发布，不支持。

---

## 2. 模型翻译：stedgeai generate

### 2.1 输入模型

**表情识别模型（ResNet18）:**

| 项目 | 值 |
|------|-----|
| 文件 | `Final AI/best_model_opset17.onnx` |
| 大小 | 42.6 MB（ONNX FP32） |
| opset | 17 |
| 输入 | 3×224×224 float32, [-1,1] 归一化 |
| 输出 | 7 float32 (logits) |
| MACs | ~1.82G |

**人脸检测模型（RFB-320）:**

| 项目 | 值 |
|------|-----|
| 文件 | `try_ai/face_detect/version-RFB-320-v11.onnx` |
| 大小 | 1.2 MB（ONNX FP32） |
| opset | 11（从 9 升级，onnxsim 简化） |
| 输入 | 3×320×240 float32 |
| 输出 | 2 个：scores + boxes |
| MACs | ~104M |

### 2.2 stedgeai 命令

```bash
# 基本命令模板
stedgeai generate \
  --model <模型.onnx> \
  --target stm32n6 \
  --st-neural-art "default@user_neuralart.json"
```

### 2.3 user_neuralart.json 配置

```json
{
    "Profiles": {
        "default": {
            "memory_pool": "./my_mpools/stm32n6.mpool",
            "options": "--enable-epoch-controller --all-buffers-info --optimization 3 --Os --Omax-ca-pipe 4 --Ocache-opt"
        }
    }
}
```

> **知识点：关键选项解释**
> - `--enable-epoch-controller`: 启用 Epoch Controller（NPU 自带的推理调度器），把模型分成 epoch block 逐个执行
> - `--all-buffers-info`: 在 network.c 中生成所有 buffer 的详细信息（地址、大小、属性）
> - `--optimization 3`: 最高优化级别（权衡面积和速度）
> - `--Os`: 优化体积（生成的 .c 文件更小）
> - `--Omax-ca-pipe 4`: 最大流水线深度 4（NPU 内部并行度）
> - `--Ocache-opt`: NPU cache 优化

### 2.4 mpool 内存池配置

mpool 是 JSON 格式的"内存地图"，告诉 stedgeai 编译器各存储区域的基地址和大小：

```json
{
  "mempools": [
    {"fname": "AXISRAM3", "offset": {"value": "834", "magnitude": "MBYTES"}, "size": {"value": "448", "magnitude": "KBYTES"}},
    {"fname": "AXISRAM4", "offset": {"value": "854464", "magnitude": "KBYTES"}, ...},
    {"fname": "AXISRAM5", ...},
    {"fname": "AXISRAM6", ...},
    {"fname": "xSPI1",   "offset": {"value": "2304", "magnitude": "MBYTES"}, "size": {"value": "30", "magnitude": "MBYTES"}},
    {"fname": "xSPI2",   "offset": {"value": "1794", "magnitude": "MBYTES"}, "size": {"value": "20", "magnitude": "MBYTES"}}
  ]
}
```

| mpool 区域 | 地址计算 | 物理含义 |
|-----------|---------|---------|
| AXISRAM3 | 834 MB = 0x34200000 | NPU 片内 SRAM bank 3 |
| AXISRAM4 | 854464 KB = 0x34270000 | NPU 片内 SRAM bank 4 |
| AXISRAM5 | 854912 KB = 0x342E0000 | NPU 片内 SRAM bank 5 |
| AXISRAM6 | 855360 KB = 0x34350000 | NPU 片内 SRAM bank 6 |
| xSPI1 | 2304 MB = 0x90000000 | HyperRAM（外部） |
| xSPI2 | 1794 MB = 0x70200000 | NOR Flash（外部） |

> **知识点：为什么 offset 用 MB？**
> stedgeai 内部使用 MB 作为地址粒度。地址 = offset × 1024 × 1024。
> 例如：1794 MB = 1794 × 0x100000 = 0x70200000

### 2.5 翻译输出文件

```
st_ai_output/
├── network.c                    ← 模型拓扑 + epoch blocks（编译进 Appli）
├── network.h                    ← I/O buffer 数量和大小宏
├── stai_network.c               ← ST AI 平台 API 实现
├── stai_network.h               ← API 声明 + 平台宏
├── network_ecblobs.h            ← Epoch Controller 二进制数据
├── network_atonbuf.xSPI2.raw    ← NOR Flash 权重（烧录用）
├── network_atonbuf.xSPI1.raw    ← HyperRAM 权重（FSBL 加载）
└── network_generate_report.txt  ← 翻译报告（MACs、内存）

st_ai_ws/
├── build_rt_network/*.o         ← NPU 运行时 .o 文件（用于链接）
└── neural_art__network/         ← 编译器内部文件
```

> **知识点：.raw 文件是什么？**
> `.raw` 是纯二进制权重数据，每个文件对应一个存储区域（xSPI1/xSPI2/AXISRAM）。
> 它们需要被烧录到对应的物理地址。`.raw` 文件名中的 `xSPI2` 表示它属于 XSPI2 地址空间。
> 编译进 Appli 的只是模型**结构**（几 KB），权重是单独烧录的。

---

## 3. 双模型共存：符号重命名

### 3.1 问题

stedgeai 为每个模型生成的代码使用相同的函数名：

```c
// 两个模型都有这些函数——链接冲突！
stai_return_code stai_network_init(stai_network *network);
stai_return_code stai_network_run(stai_network *network, ...);
const LL_Buffer_InfoTypeDef *LL_ATON_Input_Buffers_Info_network(void);
// ... 以及相同的宏:
#define STAI_NETWORK_IN_NUM (1)  // 但值不同！
#define LL_ATON_NETWORK_IN_1_SIZE_BYTES (602112)  // 表情
#define LL_ATON_NETWORK_IN_1_SIZE_BYTES (921600)  // 人脸
```

**根因**: stedgeai 使用 `C_MODEL_NAME = "network"` 作为默认模型名来生成所有符号。两个模型都是这个名，所以符号名完全重复。

### 3.2 解决方案

对**第二个模型**（人脸检测）做全局符号替换。具体命令：

```bash
# 在 face_net/ 目录下执行
for f in network.c network.h stai_network.c stai_network.h network_ecblobs.h; do
  sed -i \
    -e 's/LL_ATON_NETWORK_/LL_ATON_FACETNET_/g' \
    -e 's/stai_network/stai_facenet/g' \
    -e 's/stai_ext_network_/stai_ext_facenet_/g' \
    -e 's/STAI_NETWORK_/STAI_FACETNET_/g' \
    -e 's/"network"/"facenet"/g' \
    -e 's/_network(/_facenet(/g' \
    -e 's/_network)/_facenet)/g' \
    -e 's/_network;/_facenet;/g' \
    -e 's/_network,/_facenet,/g' \
    -e 's/__STAI_ATON_NETWORK_H/__STAI_ATON_FACETNET_H/g' \
    "$f"
done

# 修正：恢复通用类型名
sed -i 's/stai_facenet \*/stai_network */g' stai_network.c stai_network.h
sed -i 's/stai_facenet_info/stai_network_info/g' stai_network.c stai_network.h

# 修正：LL_ATON_DECLARE_NAMED 宏参数
sed -i 's/LL_ATON_DECLARE_NAMED_NN_INTERFACE(network)/LL_ATON_DECLARE_NAMED_NN_INTERFACE(facenet)/g' stai_network.c

# 修正：include 路径
sed -i 's/#include "stai_network.h"/#include "facenet_stai.h"/' stai_network.c
sed -i 's/#include "network.h"/#include "facenet_network.h"/' network.c

# 重命名头文件
mv network.h facenet_network.h
mv stai_network.h facenet_stai.h
mv network_ecblobs.h facenet_ecblobs.h
```

### 3.3 关键陷阱

| ✅ 可以改 | ❌ 不能改 |
|----------|----------|
| 函数名 `stai_network_init` → `stai_facenet_init` | **类型** `stai_network *`（它是 ST 通用类型 `typedef uint8_t`） |
| 宏 `STAI_NETWORK_IN_NUM` → `STAI_FACETNET_IN_NUM` | **结构体字段** `.network_instance.network` |
| `C_MODEL_NAME "network"` → `"facenet"` | **平台函数** `__ll_aton_stai_init_network_instance()` |
| `DECLARE_NAMED*(network)` → `DECLARE_NAMED*(facenet)` | 注释和 `@file` 标记 |

> **知识点：为什么 `stai_network` 类型不能改？**
> `stai_network` 在 `stai.h` 中定义为 `typedef uint8_t stai_network`。它是 ST AI 平台的通用句柄类型，所有模型共用。
> 把它改成 `stai_facenet` 会导致类型未定义，因为 ST 平台头文件中没有这个类型。

> **知识点：为什么 static 函数不需要改？**
> `static` 函数的作用域限定在编译单元（.c 文件）内，不同的 .o 文件中的同名 static 函数不会冲突。
> 只有非 static 的全局符号才会导致链接器报 `multiple definition`。

---

## 4. 内存规划：地址冲突解决

### 4.1 问题

两个模型经 stedgeai 翻译后，默认 xSPI2 (NOR Flash) 起始地址都是 0x70200000：

```
表情模型: xSPI2 @ 0x70200000 (20MB) ┐
人脸模型: xSPI2 @ 0x70200000 (1.2MB) ┘ ← 冲突！
```

### 4.2 分析

| 模型 | xSPI2 大小 | xSPI1 大小 | NOR Flash 总需求 |
|------|:--:|:--:|:--:|
| 表情 | 20MB | 30MB | |
| 人脸 | 1.2MB | 0MB (运行时) | |
| **合计** | **21.2MB** | 30MB | 32MB 可用 → ✅ |

NOR Flash 总容量 32MB，数据只需要 21.2MB——容量够，但地址重叠。

### 4.3 解决方案

**策略：重翻译较小的模型**（人脸检测，1.2MB），将其 xSPI2 基地址后移。

```
改前:
  表情 @ 0x70200000 (1794 MB) ┐ 冲突
  人脸 @ 0x70200000 (1794 MB) ┘

改后:
  表情 @ 0x70200000 (1794 MB) ← 不变
  人脸 @ 0x71600000 (1814 MB) ← +18MB 偏移
         ↑
    0x71600000 / 1048576 = 1814 MB
```

**步骤:**

1. 修改人脸检测的 mpool：`"xSPI2" → offset → "value": "1814"`
2. 创建新的 `user_neuralart.json` 指向新 mpool
3. 重新运行 `stedgeai generate`
4. 验证 `grep "0x716" network.c` 确认地址已更新
5. 替换工程中的 face_net/ 文件
6. 重新做符号重命名（步骤 3）

> **⚠️ 重要：stedgeai 的中文路径问题**
> `atonn.exe`（Neural-ART 编译器）内部使用 `wstring_convert::from_bytes`，**不支持中文路径**。
> 如果在 `E:\STM32N6\01_双模型主工程\` 下运行会报错：
> `terminate called after throwing an instance of 'std::range_error': wstring_convert::from_bytes`
>
> **解决**：在纯 ASCII 路径下运行 stedgeai（如 `E:\STM32N6\facenet_retranslate\`）。

### 4.4 最终内存布局

```
NOR Flash (32MB, XSPI2)
┌──────────────────────────────┐ 0x70000000
│ FSBL (~2MB)                  │
├──────────────────────────────┤ 0x70200000
│ 表情模型 bias/数据 (20MB)     │
│                              │
├──────────────────────────────┤ 0x71600000
│ 人脸检测 weights (1.2MB)     │
├──────────────────────────────┤ 0x71800000
│ 空闲 (~8MB)                  │
└──────────────────────────────┘ 0x72000000

HyperRAM (32MB, XSPI1)
┌──────────────────────────────┐ 0x90000000
│ 表情模型 weights (30MB)       │  ← FSBL 在启动时加载
│ （若做顺序执行，可先不加载）   │
│                              │
│ 人脸检测 I/O (~3.5MB)         │  ← 运行时，地址重叠！
└──────────────────────────────┘ 0x92000000
```

> **知识点：HyperRAM 冲突说明**
> 表情模型的权重（xSPI1.raw, 30MB）在启动时由 FSBL 加载到 0x90000000，
> 会覆盖人脸检测需要的 I/O 区域（0x90000000-0x90383000, ~3.5MB）。
>
> **解决方案**（待实现）：
> 1. 顺序执行：先跑人脸（HyperRAM 空闲）→ 再加载表情权重 → 再跑表情
> 2. 或重翻译表情模型使 xSPI1 起始于更高地址

---

## 5. 工程搭建：从 LED 模板到 AI 工程

### 5.1 工程结构

```
E:\STM32N6\01_双模型主工程\
├── CMakeLists.txt              ← 根 CMake（ExternalProject 包装）
├── mx-generated.cmake          ← CubeMX 生成（构建 Appli 子项目）
├── gcc-arm-none-eabi.cmake     ← 工具链文件
├── Secure_nsclib\              ← TrustZone 非安全调用接口
│   └── secure_nsc.h
├── Appli\
│   ├── CMakeLists.txt          ← ⭐ Appli 主 CMake（核心）
│   ├── mx-generated.cmake      ← CubeMX 生成（HAL 驱动 + 基础配置）
│   ├── Inc\                    ← 全局头文件
│   ├── Core\
│   │   ├── Inc\                ← HAL 配置（main.h, stm32n6xx_hal_conf.h）
│   │   ├── Src\
│   │   │   ├── main.c          ← ⭐ 应用主程序
│   │   │   ├── stm32n6xx_it.c
│   │   │   ├── stm32n6xx_hal_msp.c
│   │   │   ├── system_stm32n6xx_s.c
│   │   │   └── ...
│   │   └── Startup\
│   │       └── startup_stm32n647xx.s
│   ├── Models\
│   │   ├── expr_net\           ← 表情识别模型（原始符号名）
│   │   │   ├── network.c / network.h
│   │   │   └── stai_network.c / stai_network.h
│   │   └── face_net\           ← 人脸检测模型（FACETNET 重命名符号）
│   │       ├── network.c / facenet_network.h
│   │       └── stai_network.c / facenet_stai.h
│   ├── NPU\
│   │   ├── *.h                 ← NPU 运行时头文件（ATON.h, ll_aton.h 等）
│   │   └── Lib\
│   │       └── libnpu_runtime.a ← NPU 运行时静态库
│   └── STM32N647XX_*.ld        ← 链接脚本
```

### 5.2 模板来源

从 `E:\STM32N6\05_LED板级模板工程\`（已验证的 LED 闪烁工程）复制以下文件：

```
001_LED/ → 表情识别/
├── CMakeLists.txt
├── CMakePresets.json
├── gcc-arm-none-eabi.cmake
├── mx-generated.cmake
├── stm32n6.cfg
├── Secure_nsclib/secure_nsc.h
└── Appli/
    ├── CMakeLists.txt
    ├── CMakePresets.json
    ├── mx-generated.cmake
    ├── *.ld（所有链接脚本）
    └── Core/
        ├── Inc/*.h
        ├── Src/*.c
        └── Startup/*.s
```

> **知识点：为什么以 LED 工程为模板？**
> LED 工程是已验证的"最小可行工程"——编译→烧录→运行全链路已打通。
> 从它出发可以确保基础框架（HAL、时钟树、链接脚本、RIF）正确，
> 只需要增量添加 AI 相关代码即可。

---

## 6. NPU 运行时：静态库构建

### 6.1 来源

stedgeai 翻译时在 `st_ai_ws/build_rt_network/` 下生成 NPU 运行时 .o 文件。

### 6.2 需要的文件（15 个）

| 文件 | 职责 |
|------|------|
| `ecloader.o` | Epoch Controller 加载器 |
| `ll_aton.o` | NPU 底层 API |
| `ll_aton_cipher.o` | 权重解密 |
| `ll_aton_dbgtrc.o` | 调试追踪 |
| `ll_aton_debug.o` | 调试支持 |
| `ll_aton_lib.o` | NPU 核心算子库 |
| `ll_aton_lib_sw_operators.o` | CPU fallback 算子 |
| `ll_aton_profiler.o` | 性能分析 |
| `ll_aton_reloc_callbacks.o` | 重定位回调 |
| `ll_aton_reloc_network.o` | 网络重定位 |
| `ll_aton_rt_main.o` | ⭐ `LL_ATON_RT_Main()` 主入口 |
| `ll_aton_runtime.o` | NPU 运行时 |
| `ll_aton_util.o` | 工具函数 |
| `ll_sw_float.o` | CPU float 算子 |
| `ll_sw_integer.o` | CPU integer 算子 |
| `stedgeai_hsp_stubs.o` | 硬件平台桩函数 |

### 6.3 打包

```bash
arm-none-eabi-ar rcs libnpu_runtime.a \
  ecloader.o ll_aton.o ll_aton_cipher.o ll_aton_dbgtrc.o \
  ll_aton_debug.o ll_aton_lib.o ll_aton_lib_sw_operators.o \
  ll_aton_profiler.o ll_aton_reloc_callbacks.o ll_aton_reloc_network.o \
  ll_aton_rt_main.o ll_aton_runtime.o ll_aton_util.o \
  ll_sw_float.o ll_sw_integer.o stedgeai_hsp_stubs.o
```

### 6.4 ⚠️ 注意事项

**不要包含**：
- `network.o` — 模板示例模型的数据，会和我们的模型冲突
- `stai_network.o` — 同上
- `stedgeai_app_main.o` — 模板的 main.c（有自己的 main 函数！）
- `startup_stm32xx.o` — 模板的启动文件（我们用 CubeMX 生成的）
- `syscalls.o` / `sysmem.o` — 系统调用（我们用 HAL 提供的）

---

## 7. NPU 初始化：时钟、RIF、Cache

### 7.1 初始化 5 步法

```c
static void SystemInit_NPU(void)
{
    // ===== 第1步：使能 NPU SRAM =====
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    RCC->MEMENR |= RCC_MEMENR_AXISRAM3EN | RCC_MEMENR_AXISRAM4EN
                | RCC_MEMENR_AXISRAM5EN | RCC_MEMENR_AXISRAM6EN;
    RCC->MEMENR |= RCC_MEMENR_CACHEAXIRAMEN;

    // ===== 第2步：清除 deep-sleep 模式 =====
    // NPU SRAM 默认处于低功耗 deep-sleep，必须先唤醒
    RAMCFG_SRAM2_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM3_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM4_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM5_AXI->CR &= ~RAMCFG_CR_SRAMSD;
    RAMCFG_SRAM6_AXI->CR &= ~RAMCFG_CR_SRAMSD;

    // ===== 第3步：允许 Cache 激活 =====
    // Default value is 1, but boot may set to 0
    MEMSYSCTL->MSCR |= MEMSYSCTL_MSCR_DCACTIVE_Msk
                    | MEMSYSCTL_MSCR_ICACTIVE_Msk;

    // ===== 第4步：NPU 时钟 + Cache-AXI 时钟 =====
    NPU_Clock_Init();
}

static void NPU_Clock_Init(void)
{
    __HAL_RCC_NPU_CLK_ENABLE();
    __HAL_RCC_NPU_FORCE_RESET();
    __HAL_RCC_NPU_RELEASE_RESET();

    __HAL_RCC_CACHEAXI_CLK_ENABLE();
    __HAL_RCC_CACHEAXI_FORCE_RESET();
    __HAL_RCC_CACHEAXI_RELEASE_RESET();
}
```

### 7.2 RIF 安全配置

```c
// ===== 第5步：RIF 安全属性 =====
static void NPU_RIF_Config(void)
{
    RIMC_MasterConfig_t master_conf;
    master_conf.MasterCID = RIF_CID_1;     // CID = 1
    master_conf.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
    HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_NPU, &master_conf);
    HAL_RIF_RISC_SetSlaveSecureAttributes(
        RIF_RISC_PERIPH_INDEX_NPU,
        RIF_ATTRIBUTE_PRIV | RIF_ATTRIBUTE_SEC);
}
```

> **知识点：为什么需要 RIF 配置？**
> N6 基于 ARM TrustZone，默认所有外设互相隔离。
> 不配置 RIF，NPU 无法访问 XSPI 上的外部内存（NOR Flash 和 HyperRAM）。
> `RIMC` 配置 NPU 作为 Master 的安全等级，
> `RISC` 配置 NPU 作为 Slave 被访问时的安全属性。
> `RIF_CID_1` 是 Compartment ID，标识 NPU 属于哪个隔离域。

> **知识点：NPU 时钟架构**
> N6 的时钟树使用 PLL+IC (Intermediate Clock) 结构：
> - IC6 驱动 NPU 核心（通常来自 PLL2, 1GHz）
> - IC11 驱动 NPU RAM（AXISRAM3-6，通常来自 PLL3, 900MHz）
> - FSBL 负责初始化 PLL2/PLL3 和 IC 分频器
> - Appli 只需要 `__HAL_RCC_NPU_CLK_ENABLE()` 使能门控时钟

---

## 8. 应用代码：main.c 编写

### 8.1 头文件包含顺序

```c
#include "main.h"           // ← HAL/CMSIS 必须在最前面
#include <stdio.h>           // printf
#include <string.h>          // memcpy
#include <math.h>            // expf

/* NPU Runtime — 在 HAL 之后 include */
#include "ai_platform.h"     // AI_ALIGNED 宏
#include "ll_aton.h"         // LL_ATON_* 类型定义
#include "ll_aton_runtime.h" // LL_ATON_RT_Main() 声明

/* 模型头文件 */
#include "network.h"         // 表情: LL_ATON_NETWORK_*
#include "stai_network.h"    // 表情: stai_network_*
#include "facenet_network.h" // 人脸: LL_ATON_FACETNET_*
#include "facenet_stai.h"    // 人脸: stai_facenet_*
```

> **知识点：为什么 NPU 头文件必须在 HAL 之后？**
> NPU 平台头文件 (`ll_aton_platform.h`) 会故意覆盖 `__WFE`、`NVIC_EnableIRQ` 等宏为空，
> 这是 NPU 验证环境的预期行为（禁用中断/WFE 以避免 NPU 执行被中断打断）。
> 如果 NPU 头文件在 HAL/CMSIS 之前 include，CMSIS 定义会被覆盖掉导致各种问题。

### 8.2 声明模型实例

```c
/* 声明两个模型的 NPU 实例 */
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(network);  // 表情
LL_ATON_DECLARE_NAMED_NN_INSTANCE_AND_INTERFACE(facenet);  // 人脸
```

这个宏展开后创建：
- `NN_Instance_network` / `NN_Interface_network`（表情）
- `NN_Instance_facenet` / `NN_Interface_facenet`（人脸）

### 8.3 推理函数

```c
static void run_expression(const float *img, float *out)
{
    const uint32_t in_addr  = 0x34200000UL;   // AXISRAM3
    const uint32_t out_addr = 0x342e0000UL;   // AXISRAM5
    const uint32_t in_size  = 3 * 224 * 224 * sizeof(float);

    // 1. 拷贝输入图像
    memcpy((void *)in_addr, img, in_size);

    // 2. DCache clean: CPU 写了数据 → NPU 需要看到
    SCB_CleanDCache_by_Addr((uint32_t *)in_addr, in_size);

    // 3. 运行 NPU 推理（阻塞等待）
    LL_ATON_RT_Main(&NN_Instance_network);

    // 4. DCache invalidate: NPU 写了输出 → CPU 需要看到
    SCB_InvalidateDCache_by_Addr((uint32_t *)out_addr, 7 * sizeof(float));

    // 5. 拷贝输出
    memcpy(out, (void *)out_addr, 7 * sizeof(float));
}
```

> **知识点：为什么需要 Cache 维护？**
> CPU 和 NPU 各自有独立的数据缓存（D-Cache）。
> - **Clean**（写回）：CPU 写入了数据但可能还在 CPU cache 中，NPU 看不到。Clean 把脏数据写回内存。
> - **Invalidate**（作废）：NPU 写入了新的输出数据，但 CPU cache 中还有旧数据。Invalidate 丢弃 CPU cache 中的过期副本。
> - 对于 N6，AXISRAM 区域被配置为 non-cacheable，但为安全起见仍然做 cache 维护。

### 8.4 后处理

```c
// Softmax: 把 logits 转成概率
static void softmax7(float *v) {
    float max_val = v[0];
    for (int i = 1; i < 7; i++)
        if (v[i] > max_val) max_val = v[i];

    float sum = 0.0f;
    for (int i = 0; i < 7; i++) {
        v[i] = expf(v[i] - max_val);  // exp(x-max) 防溢出
        sum += v[i];
    }

    float inv = (sum > 0.0f) ? (1.0f / sum) : (1.0f / 7.0f);
    for (int i = 0; i < 7; i++) v[i] *= inv;
}

// Argmax: 取概率最大的类别
static int argmax7(const float *v) {
    int best = 0;
    for (int i = 1; i < 7; i++)
        if (v[i] > v[best]) best = i;
    return best;
}
```

> **知识点：Softmax 中的 exp(x - max)**
> 如果直接算 `exp(x)`，当 x 很大时（如 50），`exp(50) ≈ 5.18×10²¹`，float32 会溢出。
> 减去最大值后：`exp(50-50) = exp(0) = 1`，不会溢出，且概率比例不变（数学上等价）。

### 8.5 UART 调试输出

```c
// printf 重定向到 USART1
int _write(int fd, const char *ptr, int len) {
    (void)fd;
    for (int i = 0; i < len; i++) {
        while (!(USART1->ISR & USART_ISR_TXE_TXFNF));  // 等待 TX 就绪
        USART1->TDR = *ptr++;
    }
    return len;
}

// USART1 初始化: PE5=TX, PE6=RX, 115200-8-N-1
static void MX_USART1_UART_Init(void) {
    // ... GPIO AF7 配置 ...
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    // ... (详见 main.c)
    HAL_UART_Init(&huart1);
}
```

> **知识点：为什么是 PE5/PE6 + AF7？**
> 正点原子 N6 板子上 USART1 连接到 USB-UART 芯片（CH340），
> 对应引脚 PE5(TX) 和 PE6(RX)，复用功能为 AF7。
> STM32N6 的 GPIO 复用功能分配可能与 STM32F1/F4 不同：
> - F1/F4 上 USART1 通常在 PA9/PA10, AF7
> - N6 上 USART1 可以在 PE5/PE6, AF7（通过 GPIO matrix 灵活分配）

---

## 9. 构建系统：CMake 配置

### 9.1 N6 工程的 CMake 架构

N6 使用**双层 CMake** 结构：

```
根 CMakeLists.txt  (ExternalProject_Add)
    └── Appli/CMakeLists.txt  (实际编译)
            ├── mx-generated.cmake  (CubeMX 生成)
            └── 用户添加的源文件
```

根 CMakeLists.txt 使用 `ExternalProject_Add` 来构建 Appli。这是 N6 TrustZone 架构的要求——Boot 和 Appli 是独立的编译单元。

### 9.2 Appli/CMakeLists.txt 关键配置

```cmake
# 链接脚本：Release 用外部 Flash，Debug 用内部 SRAM
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(STM32_LINKER_SCRIPT STM32N647XX_ROMxspi2_RAMxspi1.ld)
else()
    set(STM32_LINKER_SCRIPT STM32N647XX_LRUN.ld)
endif()

# include CubeMX 生成的 HAL 驱动配置
include("mx-generated.cmake")

# 添加模型源代码
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/expr_net/network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/expr_net/stai_network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/face_net/network.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Models/face_net/stai_network.c
)

# 链接 NPU 运行时库
target_link_libraries(${CMAKE_PROJECT_NAME} npu_runtime)
```

### 9.3 常见的 CMake 问题

| 问题 | 原因 | 解决 |
|------|------|------|
| `plain signature has already been used` | mx-generated.cmake 用 plain 签名 | 统一用 plain 签名（不加 PRIVATE） |
| `AI_ALIGNED` 未定义 | 缺少 ai_platform.h include | 添加头文件或 include 路径 |
| `GPIO_AF6_USART1` 不存在 | N6 USART1 用 AF7 不是 AF6 | 查 `stm32n6xx_hal_gpio_ex.h` |

---

## 10. 烧录与测试策略

### 10.1 测试路线图

```
阶段 A: 静态图片 → 表情识别 → UART ✅ (当前)
    │ 用硬编码的 224×224 测试图像
    │ 验证: 编译→烧录→NPU推理→UART输出 全链路
    │
    ▼
阶段 B: 加入人脸检测 (待实现)
    │ 解决 HyperRAM 冲突
    │ 实现人脸检测→裁剪→表情识别 pipeline
    │
    ▼
阶段 C: 接入 OV2640 摄像头 (待实现)
    │ CubeMX 配 DCMIPP + DMA
    │ 写摄像头 I2C 初始化
    │ 实时图像采集→推理→UART 输出
```

### 10.2 烧录文件准备

```bash
# 1. 编译 Appli
cd E:\STM32N6\01_双模型主工程\build
cmake --build .

# 2. 用 CubeProgrammer 烧录
# - FSBL: 从 991 例程提取 fsbl.hex
# - Appli: Appli/build/ExprRecognition_Appli.hex
# - 表情 xSPI2 raw → NOR Flash @ 0x70200000
# - 表情 xSPI1 raw → NOR Flash → FSBL 加载到 HyperRAM
# - 人脸 xSPI2 raw → NOR Flash @ 0x71600000
```

### 10.3 无屏幕测试方法

```
开发板                           PC
┌──────────┐                   ┌──────────────┐
│ STM32N6  │──USART1(TX)──────→│ USB-UART      │
│          │                   │ 串口助手      │
│  LED闪烁 │                   │ 115200-8-N-1 │
│          │                   │              │
│  NPU推理 │                   │ 显示输出:     │
│  结果→   │                   │ "表情: Happy │
│  UART    │                   │  置信度:85%"  │
└──────────┘                   └──────────────┘
```

---

## 11. 知识点索引

| 序号 | 知识点 | 详见章节 |
|:--:|--------|:--:|
| 1 | XSPI 内存映射原理 — NOR Flash/HyperRAM 通过地址访问 | §1.2, §4.1 |
| 2 | mpool 是什么，如何修改来改变地址分配 | §2.4, §4.3 |
| 3 | stedgeai 生成文件的作用（.c, .h, .raw） | §2.5 |
| 4 | 为什么两个模型有符号冲突 + 如何重命名 | §3 |
| 5 | `stai_network` 是通用类型（typedef uint8_t），不能改 | §3.3 |
| 6 | static vs non-static 函数的链接行为 | §3.3 |
| 7 | NPU 初始化 5 步法（SRAM→Sleep→Cache→Clock→RIF） | §7 |
| 8 | Cache 维护：Clean vs Invalidate 的区别 | §8.3 |
| 9 | Softmax 中 `exp(x-max)` 的数值稳定技巧 | §8.4 |
| 10 | N6 的双层 CMake 架构（TrustZone 要求） | §9.1 |
| 11 | NPU 时钟 IC6/IC11 和 PLL 的关系 | §7.2 |
| 12 | RIF/RISAF 的安全隔离机制 | §7.2 |
| 13 | `LL_ATON_RT_Main()` 调用链（平台层→底层） | §8.2 |
| 14 | stedgeai `--enable-epoch-controller` 的作用 | §2.3 |
| 15 | stedgeai 中文路径崩溃问题的原因和解决 | §4.3 |

---

## 附录 A: 关键文件速查

| 文件 | 位置 |
|------|------|
| 最终工程 | `E:\STM32N6\01_双模型主工程\` |
| 主程序 | `Appli/Core/Src/main.c` |
| Appli CMake | `Appli/CMakeLists.txt` |
| 表情模型 | `Appli/Models/expr_net/` |
| 人脸模型 | `Appli/Models/face_net/` |
| NPU 库 | `Appli/NPU/Lib/libnpu_runtime.a` |
| 表情 raw 权重 | `try_ai/expr_final/st_ai_output/network_atonbuf.xSPI*.raw` |
| 人脸 raw 权重 | `facenet_retranslate/my_mpools/st_ai_output/network_atonbuf.xSPI2.raw` |
| LED 模板 | `001_LED/` |
| 991 AI 例程 | `991_AI_People_Detection/` |
| 旧版 SOP | `准备阶段/SOP_N6开发全流程_CLion专版.md` |
| 学习日志 | `准备阶段/LEARNING_LOG.md` |
| 讨论记录 | `准备阶段/DISCUSSION_LOG.md` |
| 项目笔记 | `准备阶段/PROJECT_NOTE.md` |

## 附录 B: 编译命令速查

```bash
# 配置
cd E:\STM32N6\01_双模型主工程
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE=../gcc-arm-none-eabi.cmake ..

# 编译
ninja -j4

# 产物
Appli/build/ExprRecognition_Appli.elf   # ELF 文件
Appli/build/ExprRecognition_Appli.hex   # HEX 文件（烧录用）
```

## 附录 C: stedgeai 重新翻译命令

```bash
# 在纯 ASCII 路径下执行！
cd E:\STM32N6\facenet_retranslate\my_mpools
stedgeai generate \
  --model "E:/STM32N6/try_ai/face_detect/version-RFB-320-v11.onnx" \
  --target stm32n6 \
  --st-neural-art "default@facenet_user_neuralart.json"
```

---

*文档版本: 2026-07-08*
*项目: STM32N6 双模型表情识别*
*作者: Liam*
