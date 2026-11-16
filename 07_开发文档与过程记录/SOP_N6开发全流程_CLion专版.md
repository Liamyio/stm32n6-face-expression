# STM32N6 开发全流程 SOP — CLion 专版

> **编写日期：** 2026-06-29
> **适用对象：** 使用 CLion + CMake 的 STM32N647 开发者
> **目标：** 翻开这份 SOP，从空目录到程序在板子上跑起来，每一步都能照做。
>
> ⚠️ **这份 SOP 和正点原子官网教程有本质区别——他们用 CubeIDE，你用 CLion。**
> 每个步骤我都标注了：🔵 = 和 CubeIDE 教程一样 / 🟡 = 和 CubeIDE 教程不同 / 🔴 = CLion 额外要做

---

## 你的实际开发环境（已经装好了）

| 工具 | 你的实际路径 | 作用 |
|------|------------|------|
| **CLion** | `E:\CLion\CLion 2025.2.5\bin\clion64.exe` | 写代码 + 编译 |
| **CubeMX** | `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX\STM32CubeMX.exe` | 图形化配芯片 + 生成代码框架 |
| **CubeProgrammer** | `D:\CubePrg\bin\STM32CubeProgrammer.exe` | 烧录（GUI） |
| **CubeProgrammer CLI** | `D:\CubePrg\bin\STM32_Programmer_CLI.exe` | 烧录（命令行备选） |
| **ARM GCC** | `E:\ST\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\` | 编译器，v13.3.1 |
| **ST-LINK GDB Server** | `E:\ST\STM32CubeCLT_1.20.0\STLink-gdb-server\bin\ST-LINK_gdbserver.exe` | 调试服务器 |
| **External Loader** | `D:\CubePrg\bin\ExternalLoader\MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr` | ✅ 已部署 |
| **FSBL** | `D:\牛马生活\【正点原子】STM32N647开发板\【正点原子】N647开发板资料盘(A盘)\4，程序源码\SoftwarePackage\SoftwarePackage\FSBL\MX25UM25645G_W958D8NBYA5I_Example\Binary\fsbl.hex` | ✅ 已就绪 |
| **CMake** | 系统 PATH | v3.28.1 |
| **Ninja** | 系统 PATH | v1.11.1 |

---

## 目录

1. [你的工具分工](#一你的工具分工)
2. [操作流程（逐阶段）](#二操作流程)
   - [A. CubeMX：新建工程](#a-cubemx新建工程)
   - [B. CubeMX：时钟配置](#b-cubemx时钟配置n6-最关键)
   - [C. CubeMX：外设配置](#c-cubemx外设配置)
   - [D. CubeMX：工程管理配置](#d-cubemx工程管理配置)
   - [E. CubeMX：生成代码](#e-cubemx生成代码)
   - [F. CLion：导入工程](#f-clion导入工程)
   - [G. CLion：链接脚本配置](#g-clion链接脚本配置n6-特有重点)
   - [H. CLion：写代码](#h-clion写代码)
   - [I. CLion：编译](#i-clion编译)
   - [J. CubeProgrammer：烧录](#j-cubeprogrammer烧录)
   - [K. 板子：运行](#k-板子运行)
3. [速查卡](#三速查卡)
4. [故障排查](#四故障排查)

---

## 一、你的工具分工

和正点原子教程最大的不同在于工具分工：

```
正点原子教程（CubeIDE 全家桶）:        你的工作流（CLion 为核心）:

  CubeMX 配芯片                          CubeMX 配芯片
     ↓                                      ↓
  CubeIDE 写代码              ≠           CLion 写代码
  CubeIDE 编译                            CLion 编译
  CubeIDE 烧录（内置）                     CubeProgrammer 烧录（独立）
  CubeIDE 调试                            CLion + GDB Server 调试
```

**核心原则：CubeMX 只出手一次（生成代码框架），之后所有工作都在 CLion 里。烧录是独立的最后一步。**

---

## 二、操作流程

---

### A. CubeMX：新建工程

> 🔵 和 CubeIDE 教程基本一样（都是 CubeMX 操作）

#### A1. 创建

```
打开 CubeMX → File → New Project
→ 搜索栏输入 "STM32N647X0H3Q" → 选中 → Start Project
→ 弹窗 "TrustZone" → 选择 "Secure domain only"
```

**为什么选 Secure domain only？** N6 有 TrustZone 安全分区。Secure only = 所有外设全权限访问，最省事。正点原子的 FSBL 也是 Secure 模式，选这个才能配合。

#### A2. 保存

```
File → Save Project As
→ 路径：E:\STM32N6\你的工程名\
→ 文件名：你的工程名.ioc
```

---

### B. CubeMX：时钟配置（N6 最关键）

> 🔵 和 CubeIDE 教程一样（都是 CubeMX 时钟配置页）

点击顶部 `Clock Configuration` 标签页。以下参数直接填：

#### B1. PLL1 主时钟（目标：CPU 600MHz）

| 参数 | 值 |
|------|-----|
| PLL1 Source | **HSI**（64MHz 内部振荡器） |
| PLL1M | **/2** |
| PLL1N | **×75** |
| PLL1P | **/4** |
| PLL1Q | **/2** |
| PLL1R | **/4** |

填完后 Clock Configuration 界面应该显示 **SYSCLK = 600 MHz**。不是 600 就回头检查。

#### B2. 总线预分频器（Prescalers）

| 预分频器 | 值 |
|----------|-----|
| **IC1** | `/1` |
| IC2 | 自动（不改） |
| IC6 | 自动（不改） |
| IC11 | 自动（不改） |

#### B3. XSPI 时钟（外部存储器）

| 配置项 | 值 |
|--------|-----|
| XSPI1_CLK 源 | **PLL1_R**（600MHz） |
| XSPI2_CLK 源 | **PLL1_R**（600MHz） |

#### B4. NPU 时钟（为 AI 做准备）

| 配置项 | 值 |
|--------|-----|
| NPU_CLK 源 | **PLL1_Q**（1200MHz） |
| NPU_CLK 分频 | **/2**（→ 600MHz） |

---

### C. CubeMX：外设配置

> 🔵 和 CubeIDE 教程一样（都是 CubeMX Pinout 页）

#### C1. 系统核心（必配，做一次即可）

在 `Pinout & Configuration` → `System Core`：

| 标签页 | 配置项 | 设置 |
|--------|--------|------|
| **CORTEX_M55_S** | ICache | ✅ 勾选 |
| **CORTEX_M55_S** | DCache | ✅ 勾选 |
| **RCC** | Supply Source | **PWR_SMPS_SUPPLY** |
| **SYS_S** | Timebase Source | **SysTick** |

**Supply Source 为什么必须是 SMPS？** 正点原子板子的供电电路是开关电源（SMPS）方案，不是 LDO。选错的话电源管理外设初始化失败。

#### C2. 外设引脚（以 LED 为例）

```
Pinout view → 找到 PG10 → 点击 → 选 GPIO_Output
Pinout view → 找到 PE10 → 点击 → 选 GPIO_Output
```

点完右键 → `Enter User Label` → 填入 `LED_R` 和 `LED_G`（可选但强烈推荐）。

---

### D. CubeMX：工程管理配置

> 🟡 和 CubeIDE 教程**不同**！你的 Toolchain 选 CMake，不是 CubeIDE

打开 `Project Manager` 标签页：

#### D1. Project 标签

| 配置项 | 设置 | 说明 |
|--------|------|------|
| Project Name | 你的工程名 | — |
| Application Structure | ✅ **勾选 Appli** | N6 双层架构必须勾 |
| **Toolchain / IDE** | ⚠️ **CMake** | ← **这里和教程不同！教程选 CubeIDE，你选 CMake** |
| Generate Under Root | ❌ **取消勾选** | 不取消结构会乱 |

#### D2. Code Generator 标签

| 配置项 | 设置 |
|--------|------|
| Copy all used libraries | ✅ |
| Generate peripheral initialization | ✅ |
| Set all free pins as analog | ✅ |

---

### E. CubeMX：生成代码

> 🟡 点 GENERATE CODE 就行，**但不要点 Open Project**（那是给 CubeIDE 用的）

```
点击 CubeMX 右上角 GENERATE CODE → 等待完成
```

**CubeMX 的 "Open Project" 按钮会打开 CubeIDE。你不需要 CubeIDE，忽略它。**

生成后在文件管理器里确认：
```
E:\STM32N6\你的工程\
├── CMakeLists.txt          ← 根 CMake（调度器）
├── Appli\
│   ├── CMakeLists.txt      ← Appli 子项目 CMake
│   ├── STM32N647XX_*.ld    ← 9 个链接脚本
│   └── Core\
│       ├── Inc\main.h
│       └── Src\main.c      ← 你写代码的主文件
├── gcc-arm-none-eabi.cmake
└── CMakePresets.json
```

---

### F. CLion：导入工程

> 🔴 **这是 CLion 额外要做的一步。** CubeIDE 用户直接打开就行，你需要手动导入。

#### F1. 首次导入

```
CLion → File → Open → 选择工程根目录（E:\STM32N6\你的工程\）
→ CLion 检测到 CMakeLists.txt
→ 弹窗 "Trust Project" → 点 Trust
→ CLion 自动读取 CMakePresets.json → Debug 和 Release 两个 Profile 自动建好
```

#### F2. 后续 CubeMX 重新生成后

```
CubeMX 改配置 → GENERATE CODE → 回到 CLion
→ CLion 顶部弹 "CMake project needs to be reloaded"
→ 点 Reload（不点的话改动不生效）
→ 继续写代码
```

**不需要重新 Open Project。CLion 一直开着就行。**

---

### G. CLion：链接脚本配置（N6 特有、重点）

> 🔴 **这是 CLion 额外要做的一步，也是全流程最关键的一步。**
> CubeIDE 用户在 GUI 里选链接脚本路径；你在 CMakeLists.txt 里用代码实现自动选择。

#### G1. 先理解：两个链接脚本决定程序跑在哪

| 链接脚本 | 代码地址 | 数据地址 | 模式 | 能独立运行？ |
|----------|---------|---------|------|:---:|
| `STM32N647XX_LRUN.ld` | 内部 SRAM (0x34...)| 内部 SRAM | Debug | ❌ 断电丢失 |
| `STM32N647XX_ROMxspi2_RAMxspi1.ld` | 外部 NOR Flash (0x90...)| 外部 HyperRAM | Release | ✅ 烧录后独立运行 |

**简单说：Debug = 临时跑在芯片 SRAM 里（ST-LINK 加载、断电消失），Release = 烧到板子上永久保存。**

#### G2. 打开 Appli/CMakeLists.txt 修改

CubeMX 生成时可能写死了 LRUN 链接脚本。找到这一行（或类似的行）：

```cmake
set(STM32_LINKER_SCRIPT STM32N647XX_LRUN.ld)
```

**替换为：**

```cmake
# === N6 链接脚本自动选择（CLion CMake Profiles 驱动）===
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    # Release: 程序存在外部 NOR Flash，FSBL 会搬到 HyperRAM 执行
    set(STM32_LINKER_SCRIPT STM32N647XX_ROMxspi2_RAMxspi1.ld)
else()
    # Debug: 程序加载到内部 SRAM，ST-LINK 直接调试
    set(STM32_LINKER_SCRIPT STM32N647XX_LRUN.ld)
endif()
```

**这个 if/else 是 CLion CMake Profiles 的枢纽：** 你在 CLion 顶部切 Debug/Release Profile → CMAKE_BUILD_TYPE 变 → 链接脚本自动跟着变。不需要每次手动改。

#### G3. 验证

在 CLion 中分别切到 Debug 和 Release Profile，分别点 🔨 Build。两个都编译通过 = 链接脚本没问题。

---

### H. CLion：写代码

> 🟡 写代码的逻辑和 CubeIDE 一样，但你用 CLion 编辑器。

#### H1. 只改 USER CODE 区域

打开 `Appli/Core/Src/main.c`，代码写在标记之间：

```c
/* USER CODE BEGIN 3 */
while (1)
{
    HAL_GPIO_TogglePin(LED_R_GPIO_Port, LED_R_Pin);   // 红灯翻转
    HAL_Delay(200);
    HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin);   // 绿灯翻转
    HAL_Delay(200);
}
/* USER CODE END 3 */
```

**为什么只能写在这里？** CubeMX 重新生成代码时会覆盖 main.c 的其他部分，但保留 `USER CODE BEGIN/END` 之间的所有内容。

#### H2. 如果你在 CubeMX 里设了 User Label

CubeMX 会在 `main.h` 自动生成宏定义：
```c
#define LED_R_Pin        GPIO_PIN_10
#define LED_R_GPIO_Port  GPIOG
```
直接用这些宏，不需要翻原理图。

---

### I. CLion：编译

> 🟡 编译用 CLion，和 CubeIDE 的 Build 功能等价，但操作入口不同。

#### I1. CLion CMake Profiles

CLion 顶部工具栏右侧有一个下拉菜单，通常显示 `Debug`：

```
┌─────────────────────────────────────────┐
│  [Debug  ▼]  [🔨]  [▶]  [🛑]           │  ← CLion 工具栏
└─────────────────────────────────────────┘
         ↑                ↑
    CMake Profile       Build 按钮
```

| Profile | CMAKE_BUILD_TYPE | 链接脚本 | 优化 | 什么时候用 |
|---------|-----------------|---------|------|-----------|
| **Debug** | Debug | LRUN.ld | -O0 | 写代码时反复验证编译（快） |
| **Release** | Release | ROMxspi2_RAMxspi1.ld | -O2 | 最终产物，拿去烧录 |

#### I2. 日常编译节奏

```
写代码
  → 切 Debug → 点 🔨（Ctrl+F9）→ 验证语法通过（通常几秒）
  → 确认无误
  → 切 Release → 点 🔨 → 生成 appli.hex
  → 拿到 build/release/Appli/appli.hex → 去 CubeProgrammer 烧录
```

#### I3. 产物位置

| 文件 | 路径 | 用在哪 |
|------|------|--------|
| `appli.hex` | `build/release/Appli/appli.hex` | **CubeProgrammer 烧录用** |
| `appli.elf` | `build/release/Appli/appli.elf` | 调试符号文件 |
| `appli.bin` | `build/release/Appli/appli.bin` | 纯二进制，备选 |

---

### J. CubeProgrammer：烧录

> 🔴 **这是 CLion 工作流和 CubeIDE 最大的不同。**
> CubeIDE 用户点 Run 就烧进去了；你需要离开 CLion，打开 CubeProgrammer，手动完成以下步骤。

**为什么不能从 CLion 直接烧？** CLion 的 Run 按钮 → ST-LINK GDB Server → 只能把程序加载到芯片内部 SRAM（对应 Debug/LRUN 模式）。N6 的程序要存到**外部 NOR Flash**，这需要 External Loader 机制——只有 CubeProgrammer 支持。

#### J1. 接线

```
两根 USB 线都要插到电脑：
  USB UART 口 → 供电
  ST-LINK 口  → 烧录
```

#### J2. 设 BOOT 模式为烧录

```
BOOT1 → 3.3V（跳线帽往外侧那根针）
BOOT0 → GND
```

#### J3. 打开 CubeProgrammer

```
启动 D:\CubePrg\bin\STM32CubeProgrammer.exe
右上角 → 选 ST-LINK → Connect
```

如果弹出黄色警告 "core is locked" → **忽略**，正常现象。

#### J4. 加载 External Loader

```
左侧 → External Loader 区域
→ 搜索框输入 "ATK-CNN647B"
→ 选中 → 加载
```

#### J5. 烧录 FSBL（只烧一次！）

```
Erase & Programming 标签
→ File Path: 浏览到：
    D:\牛马生活\【正点原子】STM32N647开发板\【正点原子】N647开发板资料盘(A盘)\
    4，程序源码\SoftwarePackage\SoftwarePackage\FSBL\
    MX25UM25645G_W958D8NBYA5I_Example\Binary\fsbl.hex
→ Start Programming
```

**FSBL = BIOS，烧一次就够了。** 除非正点原子出了新版本，否则不需要重烧。

#### J6. 烧录 Appli（改一次代码烧一次）

```
Erase & Programming 标签
→ File Path: 浏览到你的 Release .hex
    E:\STM32N6\你的工程\build\release\Appli\appli.hex
→ Start Programming
```

#### J7. 完成

```
Disconnect → 关闭 CubeProgrammer（可选）
```

---

### K. 板子：运行

> 🔵 和 CubeIDE 教程一样

#### K1. 切换 BOOT 模式

```
⚠️ 先拔掉 USB 线断电！
BOOT1 → GND  ← 改这根！
BOOT0 → GND
```

#### K2. 上电

```
插上 USB UART 线（供电）
→ 芯片上电
→ BootROM → FSBL → Appli → main() → 程序运行 ✅
```

---

## 三、速查卡

### 3.1 工具分工速查

| 步骤 | 用什么工具 | 在 CubeIDE 教程里怎么做 | 你实际怎么做 |
|------|-----------|----------------------|------------|
| 配芯片 | CubeMX | 一样 | 一样 |
| 写代码 | **CLion** | CubeIDE | **CLion 编辑器** |
| 编译 | **CLion** | CubeIDE 点 Build | **CLion 切 Profile → Ctrl+F9** |
| 选链接脚本 | **CMakeLists.txt** | CubeIDE 工程属性 GUI | **Appli/CMakeLists.txt 写 if/else** |
| 烧录 | **CubeProgrammer** | CubeIDE 点 Run | **独立打开 CubeProgrammer 手动烧** |
| 调试 | **CLion + GDB Server** | CubeIDE 点 Debug | **CLion Debug 配置 + Debug Profile** |

### 3.2 CubeIDE 教程里你需要跳过的步骤

| 正点原子教程步骤 | 为什么你不需要做 |
|-----------------|----------------|
| 在 CubeIDE 里设置 Run Configuration | 你不在 CubeIDE 里烧录 |
| 在 CubeIDE 工程属性里指定链接脚本路径 | 你用 CMakeLists.txt if/else 自动选 |
| 配置 CubeIDE 的 postbuild.sh | CMake 自动处理产物 |
| CubeIDE 的 Debug Configuration | CLion 用 GDB Server 直连 |

### 3.3 CLion 额外要做的步骤

| CLion 特有步骤 | 说明 |
|--------------|------|
| File → Open 工程目录 | CubeIDE 用户点 Open Project 就进去了 |
| 改 Appli/CMakeLists.txt | CubeIDE 用户在 GUI 里选链接脚本 |
| 切 CMake Profile | CubeIDE 用 Build Configurations |
| 手动打开 CubeProgrammer | CubeIDE 内置烧录 |
| CubeMX 重新生成后点 Reload | CubeIDE 自动刷新 |

### 3.4 BOOT 模式速查

| 操作 | BOOT0 | BOOT1 |
|------|-------|-------|
| **烧录** | GND | **3.3V** |
| **运行** | GND | **GND** |

### 3.5 编译模式速查

| Profile | 链接脚本 | 烧录后能独立跑？ | 什么时候用 |
|---------|---------|:---:|-----------|
| Debug | LRUN.ld | ❌ | 验证编译 / 调试 |
| Release | ROMxspi2_RAMxspi1.ld | ✅ | **最终烧录** |

### 3.6 关键路径速查

| 项目 | 路径 |
|------|------|
| CLion 可执行文件 | `E:\CLion\CLion 2025.2.5\bin\clion64.exe` |
| CubeMX 可执行文件 | `C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeMX\STM32CubeMX.exe` |
| CubeProgrammer 可执行文件 | `D:\CubePrg\bin\STM32CubeProgrammer.exe` |
| CubeProgrammer CLI | `D:\CubePrg\bin\STM32_Programmer_CLI.exe` |
| ARM GCC | `E:\ST\STM32CubeCLT_1.20.0\GNU-tools-for-STM32\bin\` |
| ST-LINK GDB Server | `E:\ST\STM32CubeCLT_1.20.0\STLink-gdb-server\bin\ST-LINK_gdbserver.exe` |
| External Loader | `D:\CubePrg\bin\ExternalLoader\MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr` |
| FSBL .hex | `A盘\4，程序源码\SoftwarePackage\SoftwarePackage\FSBL\MX25UM25645G_W958D8NBYA5I_Example\Binary\fsbl.hex` |
| 你的 .ioc | `E:\STM32N6\你的工程\你的工程.ioc` |
| 你的 main.c | `E:\STM32N6\你的工程\Appli\Core\Src\main.c` |
| 你的 Release .hex | `E:\STM32N6\你的工程\build\release\Appli\appli.hex` |
| 正点原子 A 盘 | `D:\牛马生活\【正点原子】STM32N647开发板\【正点原子】N647开发板资料盘(A盘)\` |

---

## 四、故障排查

| 现象 | 原因 | 解法 |
|------|------|------|
| CLion CMake 加载失败 | CMakePresets.json 里的 GCC 路径不对 | 检查 `gcc-arm-none-eabi.cmake` 里的 `TOOLCHAIN_ROOT` |
| Debug 编译通过、Release 报错 | 链接脚本 ROMxspi2_RAMxspi1 内存布局和代码不匹配 | 检查是不是选错了 .ld 文件 |
| Release 编译通过、烧进去不运行 | 用了 Debug 链接脚本 (LRUN) | 确认 Appli/CMakeLists.txt 里的 if/else 正确 |
| CubeProgrammer 连不上 | ST-LINK 线没插 | 两根 USB 都插 |
| 连上了但"No STM32 target" | 芯片没电 | USB UART 线没插（供电） |
| "External memory not found" | External Loader 没加载 | 在 CubeProgrammer 里搜 ATK-CNN647B 加载 |
| 烧完灯不亮 | BOOT 没切回 Flash Boot | BOOT1 从 3.3V 改回 GND |
| CubeMX 生成后 CLion 没反应 | 没点 Reload | CLion 顶部弹窗点 Reload |
| CLion 里看不到 .hex 文件 | 没切到 Release 编译 | 顶部切 Release Profile → 重新 Build |

---

## 十四、阶段 L：AI 模型翻译（stedgeai）

> 🔴 **这是 AI 开发独有的步骤，正点原子 CubeIDE 教程里没有。**
> 作用：把 .tflite 模型文件翻译成 N6 NPU 能执行的 C 代码 + 权重文件。

### L1. 你的工具

| 项 | 路径 |
|----|------|
| stedgeai.exe | `E:\ST\STedgeAi\4.0\Utilities\windows\stedgeai.exe` |
| 系统 PATH | 已添加 `E:\ST\STedgeAi\4.0\Utilities\windows\` |
| 验证 | `stedgeai --version` → `ST Edge AI Core v4.0.1` |

### L2. 需要的材料（每份都从正点原子 A 盘 AI 例程拷）

| 文件 | 来源 | 作用 |
|------|------|------|
| `user_neuralart.json` | `A盘\...\991_AI_People_Detection\Model\` | NPU 编译配置（优化级别、内存池位置） |
| `my_mpools/stm32n6.mpool` | 同上 `Model\my_mpools\` | N6 内存地图（定义 NOR Flash 从 0x70200000 开始） |
| 你的模型 `.tflite` | 自己训练 或 ST Model Zoo 下载 | int8 量化的 AI 模型 |

### L3. 工程目录结构

```
你的 AI 工程/
├── model.tflite               ← 你的模型
├── user_neuralart.json        ← 从 A 盘拷来，不要改
├── my_mpools/
│   └── stm32n6.mpool          ← 从 A 盘拷来，不要改
└── st_ai_output/              ← stedgeai 自动生成（每次跑会覆盖）
    ├── network.c              → 编译进 Appli
    ├── network.h              → 编译进 Appli
    ├── network_atonbuf.xSPI2.raw → 转 .hex 烧到 NOR Flash 0x70200000
    ├── stai_network.c         → 编译进 Appli
    ├── stai_network.h         → 编译进 Appli
    └── network_generate_report.txt → 查看内存分配详情
```

### L4. 执行翻译（永远记这个命令模板）

```bash
cd "你的工程目录"
stedgeai generate \
    --model 你的模型.tflite \
    --target stm32n6 \
    --st-neural-art default@user_neuralart.json
```

| 参数 | 含义 | 什么时候改 |
|------|------|-----------|
| `--model 模型.tflite` | 输入哪个模型 | 换模型时改文件名 |
| `--target stm32n6` | 目标芯片是 STM32N6 | 永远不变 |
| `--st-neural-art default@user_neuralart.json` | 用正点原子的 NPU 配置 | 永远不变 |

**以后换自己的模型，只改 `--model` 后面的文件名，其他照抄。**

### L5. 看懂输出报告的关键行

```
weights (ro)       :   11,064,337 B (10.55 MiB)     ← 权重多大
activations (rw)   :   351,232 B (343.00 KiB)        ← 推理时临时数据多大
octoFlash  [0x70200000 - ...]: 10.552 MB             ← 权重烧到 NOR Flash 的地址
npuRAM5    [0x342E0000 - ...]: 343.000 kB            ← 临时数据放 NPU 专用 RAM
```

**权重烧录地址必须和 network.c 里一致。** 用了正点原子 mpool，烧录时要用 `0x70200000`。

### L6. 转换权重为烧录格式

```bash
# .raw → .bin
cp st_ai_output/network_atonbuf.xSPI2.raw st_ai_output/network_data.xSPI2.bin

# .bin → .hex（地址 0x70200000，和 mpool 里定义的一致！）
arm-none-eabi-objcopy -I binary st_ai_output/network_data.xSPI2.bin \
    --change-addresses 0x70200000 -O ihex st_ai_output/network-data.hex
```

### L7. 产物去向

| 文件 | 下一步 |
|------|--------|
| `network.c` / `network.h` | 拷贝到 `Appli/Model/`，CMakeLists.txt 里添加编译 |
| `stai_network.c` / `stai_network.h` | 同上 |
| `network-data.hex` | CubeProgrammer 单独烧录到 NOR Flash 0x70200000 |
| `my_mpools/` + `user_neuralart.json` | 保留在工程目录，换模型时复用 |

---

> **基于：** 你的实际环境扫描（2026-06-29）+ 正点原子官方教程 + 001_LED 实战经验 + stedgeai 实战
