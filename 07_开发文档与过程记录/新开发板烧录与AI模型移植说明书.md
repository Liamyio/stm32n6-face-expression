# 新开发板烧录与 AI 模型移植说明书

适用项目：STM32N647 人脸检测与表情识别双模型系统  
适用硬件：正点原子 STM32N647 开发板、MX25UM25645G 32 MB 外部 NOR、IMX335 MIPI 摄像头

## 1. 先区分两种操作

### 1.1 新板复制现有功能

如果新开发板、外部 Flash 和摄像头型号都与当前板一致，不需要重新导入 ONNX，也不需要重新生成模型。将下面四个 HEX 一次烧入即可：

| 文件 | 烧录起始地址 | 作用 |
|---|---:|---|
| `fsbl.hex` | `0x70000000` | 第一阶段引导程序，初始化外部存储并启动应用 |
| `ExprRecognition_Appli.hex` | `0x70100400` | 双模型应用、摄像头驱动和后处理 |
| `network-data.hex` | `0x70200000` | ResNet18 表情模型权重 |
| `face_net-data.hex` | `0x71600000` | RFB-320 人脸模型权重 |

### 1.2 更换新的 AI 模型

STM32N6 不能在运行时直接加载普通 ONNX。新模型必须先经过 ST Edge AI 编译，生成：

- C 接口和网络调度代码；
- 量化参数、输入输出描述；
- 模型权重 HEX；
- NPU epoch 分配报告。

随后还要修改应用侧预处理、输出解析和内存地址。详见第 8 节。

## 2. 所需硬件

- 正点原子 STM32N647 开发板；
- ST-LINK/V3；
- USB-UART CH340；
- IMX335 MIPI 摄像头；
- 两根 USB 线：UART/供电和 ST-LINK；
- 跳帽或杜邦线，用于设置 BOOT0、BOOT1。

接线和串口配置：

- USART1 TX：PE5；
- USART1 RX：PE6；
- 波特率：115200；
- 数据格式：8-N-1；
- 串口终端编码：UTF-8。

## 3. PC 环境

安装以下软件：

1. STM32CubeProgrammer 2.20 或兼容新版；
2. STM32CubeCLT 1.20.0，包含 ARM GNU GCC 13.3、CMake、Ninja；
3. STM32Cube FW N6 1.3.0；
4. ST Edge AI Core 4.0.1，仅在重新生成 AI 模型时需要；
5. 正点原子外部 Flash Loader：
   `MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr`。

本机已验证的路径：

```text
CubeProgrammer: D:\CubePrg\bin
ARM GCC: E:\ST\STM32CubeCLT_1.20.0\GNU-tools-for-STM32
ST Edge AI: E:\ST\STedgeAi\4.0
工程: E:\STM32N6\01_双模型主工程
```

## 4. 新开发板首次烧录

### 4.1 进入烧录模式

1. 断开开发板电源；
2. BOOT0 接 GND；
3. BOOT1 接 3.3V；
4. 连接 ST-LINK；
5. 连接 USB-UART/供电；
6. 给开发板上电。

| 状态 | BOOT0 | BOOT1 |
|---|---|---|
| 烧录 | GND | 3.3V |
| 正常运行 | GND | GND |

### 4.2 检查外部 Flash Loader

确认以下文件存在：

```powershell
Test-Path "D:\CubePrg\bin\ExternalLoader\MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr"
```

应返回 `True`。

### 4.3 一次烧录四个文件

在 PowerShell 执行：

```powershell
cd D:\CubePrg\bin

.\STM32_Programmer_CLI.exe `
  -c port=SWD `
  -el "ExternalLoader/MX25UM25645G_ATK-CNN647B_ExtMemLoader.stldr" `
  -d "E:\STM32N6\01_双模型主工程\fsbl.hex" `
  -d "E:\STM32N6\01_双模型主工程\Appli\build\ExprRecognition_Appli.hex" `
  -d "E:\STM32N6\01_双模型主工程\network-data.hex" `
  -d "E:\STM32N6\01_双模型主工程\face_net-data.hex" `
  -v
```

有多个 ST-LINK 时，在连接参数中加入序列号：

```text
-c port=SWD sn=E1007200D0D2139393740544
```

烧录成功的关键提示：

```text
File download complete
Download verified successfully
```

### 4.4 切换到运行模式

1. 烧录完成后断电；
2. BOOT1 从 3.3V 改接 GND；
3. BOOT0 保持 GND；
4. 确认 IMX335 排线方向正确；
5. 重新上电；
6. 打开 USART1，115200-8-N-1，UTF-8。

## 5. 新板验收

正常启动应依次看到：

```text
[初始化] STAI运行时初始化成功
[摄像头] 初始化IMX335...
[摄像头] 初始化成功
[摄像头] 启动连续采集...
[摄像头] AE/AWB预热完成
[人脸] 检测中...
[人脸] 检测到 1 张人脸
[表情] 结果: 开心 (...%)
[总耗时] ... ms
```

验收要求：

- 摄像头初始化无错误；
- 对准真人后能检测到人脸；
- 七类表情中文输出正常；
- 连续运行至少 30 分钟，无 HardFault、PIPE2 timeout 或 I2C 错误；
- 当前参考性能约为人脸 12 ms、表情 37 ms、整帧 101 ms。

## 6. 修改源码后重新编译

首次在新电脑配置：

```powershell
cd "E:\STM32N6\01_双模型主工程"

cmake -S Appli -B Appli\build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  "-DCMAKE_TOOLCHAIN_FILE=E:/STM32N6/01_双模型主工程/gcc-arm-none-eabi.cmake"

cmake --build Appli\build --parallel 4
```

以后只需：

```powershell
cmake --build "E:\STM32N6\01_双模型主工程\Appli\build" --parallel 4
```

校验固件：

```powershell
powershell -ExecutionPolicy Bypass `
  -File "E:\STM32N6\01_双模型主工程\tools\verify_firmware_artifacts.ps1"
```

必须看到 `Artifact verification passed.`。编译后通常只需要重烧应用 HEX；模型代码或模型权重改变时，必须同时更新对应模型数据 HEX。

## 7. 四个烧录文件为什么必须匹配

应用代码中保存着模型输入输出、epoch、权重地址和量化参数。模型权重 HEX 与生成的 C 代码属于同一次 ST Edge AI 输出，不能混用不同版本。

发布固件时应把以下文件作为一个版本保存：

```text
fsbl.hex
ExprRecognition_Appli.hex
network-data.hex
face_net-data.hex
```

推荐同时记录文件大小、生成日期和 SHA-256：

```powershell
Get-FileHash "E:\STM32N6\01_双模型主工程\fsbl.hex" -Algorithm SHA256
Get-FileHash "E:\STM32N6\01_双模型主工程\Appli\build\ExprRecognition_Appli.hex" -Algorithm SHA256
Get-FileHash "E:\STM32N6\01_双模型主工程\network-data.hex" -Algorithm SHA256
Get-FileHash "E:\STM32N6\01_双模型主工程\face_net-data.hex" -Algorithm SHA256
```

## 8. 更换 AI 模型的完整流程

### 8.1 软件组交付要求

新模型至少应提供：

- 静态 QDQ INT8 ONNX；
- 固定输入 shape；
- opset 17 或经 ST Edge AI 验证的版本；
- 输入布局 NCHW/CHW；
- RGB/BGR 顺序；
- 输入归一化、scale、zero point；
- 每个输出 tensor 的名称、shape、scale、zero point；
- 类别顺序；
- 代表性校准数据和 PC 精度报告；
- 原始 FP32 模型用于回归比较。

不要交付仅包含 `DynamicQuantizeLinear`、`ConvInteger` 或 `MatMulInteger` 的动态量化模型。当前工具链对静态 QDQ INT8 的支持和 NPU 利用率更可靠。

### 8.2 在 ST Edge AI 中分析模型

先执行分析，不要直接替换工程：

```powershell
stedgeai analyze --model "新模型.onnx" --target stm32n6
```

检查：

- 模型是否能完整解析；
- NPU HW epoch 占比；
- Hybrid/SW epoch 数量；
- 激活内存是否小于可用 SRAM；
- 输入输出量化参数是否符合训练定义。

### 8.3 生成 STM32N6 代码和权重

使用项目对应的 `user_neuralart.json`/memory pool 配置执行 `stedgeai generate`。表情模型使用网络名 `network`，人脸模型使用独立前缀 `facenet`，避免两套生成代码符号冲突。

生成后需要保存：

- `network.c/.h` 或 `facenet_network.c/.h`；
- `stai_network.c/.h`；
- 生成报告和 `c_info.json`；
- 权重 HEX；
- memory pool 文件。

当前可参考：

```text
03_表情模型INT8生成产物
04_人脸模型INT8生成产物
01_双模型主工程\Appli\Models\expr_net
01_双模型主工程\Appli\Models\face_net
```

### 8.4 替换板端生成代码

表情模型文件放入：

```text
01_双模型主工程\Appli\Models\expr_net
```

人脸模型文件放入：

```text
01_双模型主工程\Appli\Models\face_net
```

替换前备份整个目录。不要只替换某一个 `.c` 或 `.h`，生成代码必须整套一致。

### 8.5 修改应用接口

在 `Appli/Core/Src/main.c` 中同步检查：

1. 输入宽高和通道布局；
2. RGB/BGR；
3. INT8 scale 和 zero point；
4. 输入缓冲大小与 32 字节对齐；
5. 输出 tensor 数量、顺序和大小；
6. 人脸 anchor 解码、score 阈值和 NMS；
7. 表情类别顺序与反量化公式；
8. 模型 context size 和 runtime 隔离区；
9. 权重 HEX 地址是否与其他区域重叠。

当前表情模型必须保留 `runtime_slack[256]` 和每帧 context 重建逻辑。更换 runtime 或模型后，应重新验证是否仍需要，并在确认前不要删除。

### 8.6 生成并烧录新权重 HEX

为新权重分配固定、互不重叠的外部 Flash 地址。当前布局：

```text
0x70000000  FSBL
0x70100400  应用
0x70200000  表情权重
0x71600000  人脸权重
```

新 HEX 的扩展线性地址必须与代码内权重地址一致。生成完成后重新编译应用，并同时烧录应用和改变过的模型 HEX。

### 8.7 PC 与板端回归

按以下顺序测试：

1. PC ONNXRuntime 使用固定图片得到基准输出；
2. PC 复现板端裁剪、RGB/CHW、量化和后处理；
3. 板端使用同一张静态图片验证输出；
4. 再接摄像头测试；
5. 比较 top-1、概率、框坐标和耗时；
6. 连续运行 30 分钟以上。

## 9. 常见问题

### SWD 无法连接

- 检查 BOOT1 是否为 3.3V；
- 检查 ST-LINK 供电和 SWD 接线；
- 指定正确 ST-LINK 序列号；
- 断电后重新进入烧录模式。

### External Loader 报错

- Loader 型号必须是 `MX25UM25645G_ATK-CNN647B`；
- 确认开发板外部 Flash 型号一致；
- 检查 Loader 路径和 CubeProgrammer 版本。

### 烧录成功但不上电运行

- BOOT1 必须从 3.3V 改回 GND；
- 必须烧录 FSBL；
- 应用、模型权重和生成代码必须是同一版本。

### 摄像头初始化失败

- 检查 IMX335 排线方向和接触；
- 检查 I2C2；
- 检查摄像头供电和 MIPI 时钟；
- 不能将其他摄像头驱动直接用于 IMX335。

### 能运行但检测不到人脸

- 确认摄像头输出是 RGB888 HWC；
- 确认模型输入是 CHW INT8；
- 检查 RGB/BGR、scale、zero point；
- 等待 AE/AWB 预热；
- 先用 PC 固定图片验证预处理。

### 中文乱码

- 串口终端选择 UTF-8；
- 波特率设为 115200；
- 开机最前面的少量乱码可能来自复位和波特率稳定前的字节。

## 10. 移植到不同开发板的边界

如果只是另一块相同型号正点原子 STM32N647 板，可以直接使用本说明第 4 节。

如果更换为其他 STM32N6 板卡、其他外部 Flash 或其他摄像头，则不能直接烧录现有四个 HEX，至少需要重新处理：

- FSBL 和外部 Flash 初始化；
- CubeProgrammer External Loader；
- linker script 和外部存储地址；
- RIF/RISAF 和 NPU cache；
- GPIO、I2C、UART；
- CSI lane、摄像头时钟、DCMIPP/ISP；
- 模型权重地址和内存池。

完成这些板级适配后，AI 模型和后处理代码才可以复用。

## 11. 最终检查表

- [ ] 硬件型号与当前工程一致
- [ ] BOOT0=GND、BOOT1=3.3V 进入烧录模式
- [ ] External Loader 型号正确
- [ ] 四个 HEX 全部烧录并 verify 成功
- [ ] BOOT1 改回 GND
- [ ] 串口 115200-8-N-1、UTF-8
- [ ] IMX335 初始化成功
- [ ] 人脸检测和表情识别均有结果
- [ ] 连续运行 30 分钟无故障
- [ ] 发布时保存四个 HEX 的版本与 SHA-256
