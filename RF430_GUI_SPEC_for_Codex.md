# RF430FRL152H + PN5180 Web GUI 开发说明（交给 Codex）

> 目标：在**保留当前已经跑通的 ESP32-C3 + PN5180 + RF430FRL152H ISO15693 电阻测量链路**的前提下，开发一个浏览器图形化调试/测量工具。本文把 TI 官方资料中真正需要落到代码里的寄存器、位定义、数据格式、硬件约束，以及当前项目已经验证过的事实整理出来，供 Codex 直接实现。

---

## 1. 当前项目基线：不得破坏

### 1.1 MCU 与 PN5180 固定引脚

```cpp
#define PN5180_NSS   3
#define PN5180_BUSY  5
#define PN5180_RST   4

#define PN5180_SCK   6
#define PN5180_MISO  7
#define PN5180_MOSI  10
```

平台：
- ESP32-C3 SuperMini
- PlatformIO
- Arduino Framework
- 串口 115200
- PN5180 通过 SPI 与 ESP32-C3 通信
- PN5180 与 RF430FRL152H 通过 ISO/IEC 15693 通信

### 1.2 PN5180 库

使用 Andreas Trappmann 的完整仓库：

```ini
lib_deps =
    https://github.com/ATrappmann/PN5180-Library.git
```

主要头文件：

```cpp
#include <PN5180.h>
#include <PN5180ISO15693.h>
```

当前已经验证使用的接口：

```cpp
getInventory()
getSystemInfo()
readSingleBlock()
writeSingleBlock()
setupRF()
reset()
```

### 1.3 已知兼容性注意事项

1. `PN5180ISO15693::strerror()` 在 ESP32 环境中会因为 `errno` 宏冲突产生编译问题。当前工程直接打印错误码，不依赖 `strerror()`。
2. 当前工程使用自定义 SPI 引脚。不要随意改变已经验证成功的 SPI 初始化流程：
   ```cpp
   SPI.begin(PN5180_SCK, PN5180_MISO, PN5180_MOSI, PN5180_NSS);
   ```
3. PN5180 老库存在阻塞等待风险。GUI 版本必须考虑 BUSY timeout、reset timeout、RF430 移出磁场后的恢复，以及 ISO15693 失败不能拖死 Web 服务。
4. 不要为了“代码漂亮”先重写底层通信。先封装当前能工作的逻辑，再逐步替换。

---

## 2. 当前已验证状态

实际读取到：

```text
UID = E0:07:A2:00:00:0A:DF:8D
Block size = 8 bytes
Block count = 243
```

已验证：
- Inventory
- UID
- Get System Info
- Block 读
- Block 写
- ROM Sensor Firmware 启动
- ADC1 + ADC2 电阻测量
- Block 9 数据解析

最近一次实测：

```text
ADC1 strain RAW   = 5320
ADC2 200k ref RAW = 10498
Rref = 200000 Ω
Rsensor ≈ 101352.64 Ω
```

下一次：

```text
ADC1 = 5314
ADC2 = 10500
Rsensor ≈ 101.219 kΩ
```

这组数据作为 GUI 重构后的回归测试基线。

---

# 3. RF430FRL152H 硬件必须知道的事实

来源：TI `RF430FRL15xH NFC ISO 15693 Sensor Transponder Datasheet`，SLAS834C。

## 3.1 模拟相关引脚

| 引脚 | 作用 |
|---|---|
| Pin 13 | ADC0 |
| Pin 15 | SVSS，Sensor Reference Potential |
| Pin 17 | ADC1 / TEMP1 / Resistive Bias 1 |
| Pin 18 | ADC2 / TEMP2 / Resistive Bias 2 |

TI ROM Firmware 标准角色：

```text
ADC1 / TEMP1 → Reference resistor
ADC2 / TEMP2 → Thermistor
ADC0         → General analog input
```

但当前 PCB：

```text
ADC1 → 应变/压阻传感器
ADC2 → 200 kΩ 参考电阻
```

因此 GUI 必须允许 ADC1 / ADC2 的逻辑角色交换。

## 3.2 SVSS

当前正确连接：

```text
SVSS ── 1 µF ── VSS/GND
```

数据手册典型应用给出：
- `C7 = 1 µF`，SVSS 与 VSS 间旁路电容
- `R2 = 100 kΩ`，典型参考电阻

SVSS 不是普通 GND。数据手册给出的 SVSS 典型输出约 125 mV，范围约 80–165 mV。

---

# 4. ROM Sensor Firmware 工作方式

来源：SLAU603B。

RF430 默认 ROM Sensor Firmware 支持：
- Reference / ADC1
- Thermistor / ADC2
- ADC0
- Internal temperature sensor
- Digital sensors

控制路径：

```text
PN5180
  ↓ ISO15693 Write Block
RF430 FRAM virtual registers
  ↓
ROM Sensor Firmware
  ↓
ADC sampling
  ↓
FRAM sample buffer
  ↓ ISO15693 Read Block
PN5180
```

当前 GUI 项目不需要先给 RF430 烧自定义固件。

---

# 5. 8-byte Block 0：核心控制寄存器

Block 0 起始地址：

```text
0xF868
```

| Byte | 地址 | 寄存器 | 主要 GUI 功能 |
|---:|---:|---|---|
| 0 | F868 | General Control | Start / Reset |
| 1 | F869 | Status | 状态 |
| 2 | F86A | Sensor Control | 通道选择 |
| 3 | F86B | Frequency | 采样周期 + Pass 高位 |
| 4 | F86C | Number of Passes | Pass 低 8 位 |
| 5 | F86D | Averaging | ROM 平均 |
| 6 | F86E | Interrupt Control | Infinite Sampling |
| 7 | F86F | Error Control | UsingThermistor |

---

# 6. General Control：Block 0 Byte 0

地址 `F868`。

| Bit | 名称 | 作用 |
|---:|---|---|
| 0 | Start | 写 1 启动新的采样过程 |
| 1 | PowerMode | Idle power mode |
| 2 | ControlPower | Battery switch control enable |
| 3 | SetPower | Battery switch state |
| 4 | Reserved | 必须写 0 |
| 5 | ControlInterrupt | 手动中断控制 |
| 6 | SetInterrupt | interrupt pin |
| 7 | Reset | 软件复位 |

要求：
- 其他配置先写好，再触发 Start。
- 不要在 Sampling 中重复 Start。
- GUI 应把“Apply Config”和“Start Sampling”分开。

---

# 7. Status：Block 0 Byte 1

地址 `F869`。

```text
bits 1:0 = State
```

| State | 含义 |
|---:|---|
| 0 | Idle |
| 1 | Sampling in progress |
| 2 | Data available in FRAM |
| 3 | Error |

其他重要位：
- bit2 Overflow
- bit3 Timing Error
- bit4 EndOfSampleInt
- bit5 BIP8ErrorInt
- bit6 NFCBridgeRXInt
- bit7 ThresholdInterrupt

GUI 出现 State=3 时，不允许继续把旧 Block 9 当成新数据。

---

# 8. Sensor Control：Block 0 Byte 2

地址 `F86A`。

| Bit | 通道 |
|---:|---|
| 0 | Reference / ADC1 |
| 1 | Thermistor / ADC2 |
| 2 | ADC0 |
| 3 | Internal sensor |
| 4 | Digital sensor 1 |
| 5 | Digital sensor 2 |
| 6 | Digital sensor 3 |
| 7 | Reserved |

例：

```text
0x01 = ADC1
0x02 = ADC2
0x03 = ADC1 + ADC2
0x04 = ADC0
0x07 = ADC1 + ADC2 + ADC0
```

当前电阻测量：

```cpp
SensorControl = 0x03;
```

GUI 用复选框生成，不让用户手算 hex。

---

# 9. Frequency：Block 0 Byte 3

地址 `F86B`。

低 5 位：

| 值 | 周期 |
|---:|---|
| 0 | 4 次/秒 |
| 1 | 2 次/秒 |
| 2 | 每 1 秒 |
| 3 | 每 5 秒 |
| 4 | 每 15 秒 |
| 5 | 每 30 秒 |
| 6 | 每 1 分钟 |
| 7 | 每 2 分钟 |
| 8 | 每 5 分钟 |
| 9 | 每 10 分钟 |
| 10 | 每 30 分钟 |
| 11 | 每 1 小时 |
| 12 | 每 2 小时 |
| 13 | 每 5 小时 |
| 14 | 每 10 小时 |
| 15 | 每 24 小时 |
| 16 | Custom time |

Custom time 由 Custom Timer Value Register 提供，单位 ms。

重要：`F86B bits7:5` 是 Number of Passes 的高 3 位，所以不能直接覆盖整个 byte。

推荐：

```cpp
block0[3] =
    (block0[3] & 0xE0) |
    (frequency & 0x1F);
```

---

# 10. Number of Passes

低 8 位：`F86C`  
高 3 位：`F86B bits7:5`

完整宽度 11 bit。

简单无 skip 时：

```text
Total samples =
Number of Passes × Averaging × Number of selected sensors
```

---

# 11. Averaging：Block 0 Byte 5

地址 `F86D`。

```text
1～255
```

- 1 = 不平均
- >1 = ROM 内部做平均后再存储

GUI 必须区分：
- RF430 ROM Averaging
- 网页显示的滑动平均 / smoothing

---

# 12. Interrupt Control：Block 0 Byte 6

地址 `F86E`。

最重要：

```text
bit0 = Infinite Sampling
```

TI 要求 Infinite Sampling 时：

```text
Number of Passes = 2
```

停止无限采样：清 Start。

第一版 GUI 只开放 Infinite Sampling，其他 interrupt 位放 Advanced。

---

# 13. Error Control：Block 0 Byte 7

地址 `F86F`。

| Bit | 名称 |
|---:|---|
| 0 | VDD2XLow |
| 1 | VDDBLow |
| 2 | BIP8Enable |
| 3 | ResetError |
| 4 | BusTestMode |
| 5 | RAMStorage |
| 6 | UsingThermistor |
| 7 | WatchdogEnabled |

当前电阻测量关键：

```text
bit6 = UsingThermistor
```

- 0：不打开 reference / thermistor 电阻偏置电流
- 1：打开电阻偏置输出

当前必须：

```cpp
ErrorControl |= 0x40;
```

UI 建议名称：

```text
Resistive Bias / UsingThermistor
```

---

# 14. 电阻偏置电流

SLAU603B：在 thermistor/resistive 模式下，reference 与 thermistor 引脚施加约 2.4 µA 电流。

实际电流存在器件差异，所以最终电阻不要只用名义 2.4 µA 计算，优先使用已知参考电阻的 ADC 比值。

---

# 15. Block 2：ADC 配置

8-byte Block 2：

| Byte | 地址 | 通道 |
|---:|---:|---|
| 0 | F878 | ADC1 / Reference |
| 1 | F879 | ADC2 / Thermistor |
| 2 | F87A | ADC0 |
| 3 | F87B | Internal |

---

# 16. ADC Config Byte 位定义

```text
bits 1:0 = Gain
bit 2    = Filter Type
bits 5:3 = Oversampling / Decimation
bit 6    = Virtual Ground selection
bit 7    = Reserved
```

## 16.1 Gain

```text
00 = ×1
01 = ×2
10 = ×4
11 = ×8
```

## 16.2 Filter

```text
0 = CIC
1 = Moving Average
```

## 16.3 CIC

| bits5:3 | R |
|---|---:|
| 000 | 32 |
| 001 | 64 |
| 010 | 128 |
| 011 | 256 |
| 100 | 512 |
| 101 | 1024 |
| 110 | 2048 |
| 111 | invalid |

## 16.4 Moving Average

| bits5:3 | Samples |
|---|---:|
| 000 | 4096 |
| 001 | 8192 |
| 010 | 16384 |
| 011 | 32768 |
| 100～111 | invalid |

GUI 必须根据 Filter 动态切换合法选项。

---

# 17. 转换时间与精度

CIC：

| R | 参考有效精度 | Firmware time |
|---:|---:|---:|
| 32 | 7 bit | 32 ms |
| 64 | 9 bit | 64 ms |
| 128 | 10 bit | 128 ms |
| 256 | 12 bit | 256 ms |
| 512 | 13 bit | 512 ms |
| 1024 | 15 bit | 1024 ms |
| 2048 | 16 bit | 2048 ms |

Moving Average：

| Samples | 官方描述精度 | Firmware time |
|---:|---|---:|
| 4096 | <14 bit | 2.048 s |
| 8192 | 14 bit | 4.096 s |
| 16384 | >14 bit | 8.192 s |
| 32768 | >14 bit | 16.384 s |

Preset 必须基于这些合法配置生成，不能直接硬编码不明 hex。

---

# 18. Virtual Ground

```text
bit6 = 0 → SVSS
bit6 = 1 → AVSS
```

当前硬件使用 SVSS，所以当前 bit6=0。

---

# 19. ADC 编码器

必须统一编码，不散落 magic number：

```cpp
enum class AdcGain : uint8_t {
    X1 = 0,
    X2 = 1,
    X4 = 2,
    X8 = 3
};

enum class AdcFilter : uint8_t {
    CIC = 0,
    MovingAverage = 1
};

enum class AdcReference : uint8_t {
    SVSS = 0,
    AVSS = 1
};

uint8_t encodeAdcConfig(
    AdcGain gain,
    AdcFilter filter,
    uint8_t rateBits,
    AdcReference reference)
{
    return
        (static_cast<uint8_t>(gain) & 0x03)
        | ((static_cast<uint8_t>(filter) & 0x01) << 2)
        | ((rateBits & 0x07) << 3)
        | ((static_cast<uint8_t>(reference) & 0x01) << 6);
}
```

当前：

```text
Gain ×1 + CIC 256 + SVSS
→ 0x18
```

`×2 + CIC256 + SVSS → 0x19`。

---

# 20. Current Working ADC Preset

当前已经跑通：

```cpp
ADC1 config = 0x18;
ADC2 config = 0x18;
```

即：

```text
Gain = ×1
Filter = CIC
R = 256
Reference = SVSS
```

当前 Block 2：

```text
18 18 00 05 00 FF FF FF
```

重构后的默认 preset 必须保留。

---

# 21. ADC RAW 与电压

采样数据是 ADC 原始值。

工程换算：

```cpp
float adcRawToVoltage(uint16_t raw, float gain)
{
    raw &= 0x3FFF;
    return (raw * 0.9f) / (16383.0f * gain);
}
```

规则：
- 14-bit 满量程：`0x3FFF`
- `0` / `0x3FFF` 对电阻测量视为异常或边界，不继续算电阻。

---

# 22. 电阻计算

TI ROM 标准语义：

```text
ADC1 = Reference
ADC2 = Thermistor / Unknown
```

标准比值：

```text
Runknown =
Rreference × ADC_unknown / ADC_reference
```

但当前 PCB：

```text
ADC1 = Sensor
ADC2 = 200 kΩ Reference
```

所以当前必须：

```cpp
Rsensor =
    Rref *
    ADC1_RAW /
    ADC2_RAW;
```

GUI 必须允许：

```text
ADC1 role = Sensor / Reference / Ignore
ADC2 role = Sensor / Reference / Ignore
```

根据角色自动决定公式。

---

# 23. 当前参考电阻

默认：

```text
Rref = 200000 Ω
```

GUI 允许输入万用表实测值，例如：

```text
199843.2 Ω
```

---

# 24. Block 9：数据起点

默认 Sensor Data Storage 起始地址：

```text
F8B0
```

8-byte 模式：

```text
Block 9
```

样本：
- 连续存储
- 16-bit
- raw ADC
- little-endian

例如：

```text
Block 9:
C8 14 02 29 FF FF FF FF
```

解析：

```text
ADC1 = 0x14C8 = 5320
ADC2 = 0x2902 = 10498
```

---

# 25. 数据顺序

数据按启用传感器的采样顺序连续存储。

ADC1 + ADC2 时：

```text
Byte0-1 → ADC1
Byte2-3 → ADC2
```

如果还启用 ADC0，后面继续存 ADC0。

GUI 不能永远假设 Block 9 的前 4 byte 就一定是 ADC1 + ADC2；应根据 SensorControl 自动建立 sample layout。

---

# 26. 数据有效性

```cpp
raw &= 0x3FFF;
```

至少检查：

```text
0x0000 → invalid / warning
0x3FFF → saturated
```

Reference 为 0：禁止除法。  
Reference 饱和：禁止电阻计算。

---

# 27. RF 场噪声

TI 明确提醒：RF reader 的 RF field 存在时会降低 ADC 精度。

GUI 建议提供：
- 原始值
- 均值
- 标准差
- Min/Max
- 滑动平均
- 电阻值
- ΔR
- ΔR/R0

---

# 28. GUI 功能

## Device

显示：

```text
PN5180 status
RF430 detected
UID
Block size
Block count
State
Overflow
Timing Error
```

## ADC Configuration

ADC1 / ADC2 / ADC0 / Internal 分别设置：

```text
Enable
Gain
Filter
Rate
Reference
Config Byte preview
```

## Measurement

```text
Single
Start
Stop
Continuous
Number of Passes
RF430 Averaging
Frequency
Custom Timer
Infinite Sampling
Resistive Bias
```

## Resistance

```text
ADC role
Rref
R0
Gauge Factor
ADC RAW
Resistance
ΔR
ΔR/R0
strain
```

应变：

```text
strain = (ΔR/R0) / GF
```

显示 `µε`。

## Live Chart

```text
Resistance vs time
ΔR/R0 vs time
ADC1 RAW
ADC2 RAW
```

建议最近 500～1000 点，支持：
- Start
- Stop
- Clear
- Set R0
- Export CSV

实时传输：WebSocket 或 SSE。

## Advanced

默认只读：
- Read Block
- Block 0～15 hex dump

Raw Write 默认关闭；如果开放，必须危险确认 + read-back verify。

---

# 29. 安全限制

1. 不允许默认任意写整片 FRAM。
2. 不允许误写 patch / interrupt vectors / boot 数据。
3. 不允许 RF430 离开磁场时 Web 服务卡死。
4. 不允许无限 BUSY wait。
5. 不允许 sampling error 后读取旧数据冒充新数据。
6. 不允许非法 ADC Filter/Rate 组合。
7. 不允许 Infinite Sampling 而 Passes 未设为 2。
8. 不允许覆盖 Frequency byte 高 3 位。
9. 不允许 Reference=0 继续计算。
10. 不允许大量 magic number。

---

# 30. 推荐代码结构

```text
src/
├── main.cpp
├── pn5180_transport.h
├── pn5180_transport.cpp
├── rf430_device.h
├── rf430_device.cpp
├── rf430_registers.h
├── rf430_config.h
├── rf430_config.cpp
├── measurement.h
├── measurement.cpp
├── web_api.h
├── web_api.cpp
└── settings.cpp/.h

data/
├── index.html
├── app.js
└── style.css
```

职责：
- `pn5180_transport`：Inventory/SystemInfo/Read/Write/timeout/recovery
- `rf430_registers`：地址、block、offset、mask、enum
- `rf430_config`：encode/decode、Block0/Block2 builder、validation
- `measurement`：启动、poll state、解析 sample、电阻、R0、strain
- `web_api`：HTTP + WebSocket/SSE + JSON

---

# 31. 配置 Preview

GUI 修改参数：

```text
→ encoder
→ Preview Block 0 / Block 2
→ 用户 Apply
→ Write Block 2
→ read-back verify
→ Start 时最后写 Block 0 Start
```

例：

```text
Block 0:
01 00 03 03 01 01 00 40

Block 2:
18 18 00 05 00 FF FF FF
```

---

# 32. Web API 建议

```text
GET  /api/status
GET  /api/config
POST /api/config
POST /api/apply
POST /api/sample
POST /api/start
POST /api/stop
GET  /api/block?id=9
POST /api/baseline
GET  /api/export.csv
```

实时：

```text
/ws
```

JSON 示例：

```json
{
  "timestamp_ms": 12500,
  "state": "data_ready",
  "adc1_raw": 5320,
  "adc2_raw": 10498,
  "sensor_ohm": 101352.64,
  "r0_ohm": 101280.0,
  "delta_r_ohm": 72.64,
  "relative_change": 0.000717,
  "strain_ue": 341.4
}
```

---

# 33. Wi-Fi

第一版建议 ESP32-C3 自建 AP：

```text
SSID: RF430-Tool
```

避免依赖校园网。

前端可使用 LittleFS/SPIFFS 或 PROGMEM。独立 `data/` 更利于开发。

---

# 34. Preferences / NVS

保存：
- ADC config
- role mapping
- Rref
- R0
- GF
- averaging
- frequency
- chart settings

但启动时：

```text
Load preset → 只显示 → 用户 Apply 后才写 RF430
```

不要启动后自动覆盖 RF430。

---

# 35. Presets

可提供：

```text
Current Working
Fast
Balanced
Low Noise
Custom
```

Current Working 必须：

```text
ADC1 = ×1 / CIC256 / SVSS
ADC2 = ×1 / CIC256 / SVSS
ADC1 role = Sensor
ADC2 role = Reference
Rref = 200000 Ω
SensorControl = ADC1 + ADC2
UsingThermistor = ON
```

真实回归基线：

```text
ADC1 ≈ 5314～5320
ADC2 ≈ 10498～10500
R ≈ 101.2～101.4 kΩ
```

其他 preset 必须用参数编码器生成，不能直接堆 hex。

---

# 36. Codex 开发阶段

## Phase 0：备份

把现有成功代码复制：

```text
baseline_working.cpp
```

禁止修改。

## Phase 1：只分析

先输出：
- 当前流程
- Block / Register map
- 文件架构
- GUI 页面
- API
- 风险

不改代码。

## Phase 2：驱动封装

封装 PN5180 / RF430 read/write，继续保持串口回归测试通过。

## Phase 3：寄存器配置模型

实现：
- `encodeAdcConfig`
- `decodeAdcConfig`
- `buildBlock0`
- `buildBlock2`
- 合法性检查

测试：

```text
×1+CIC256+SVSS → 0x18
×2+CIC256+SVSS → 0x19
```

## Phase 4：Measurement Service

实现：
- sample once
- poll state
- Block9 layout
- raw parse
- resistance
- R0
- strain
- stats

先串口验收。

## Phase 5：Web API

HTTP + WebSocket/SSE。

## Phase 6：Frontend

最后做 GUI 和图表。

## Phase 7：回归

Current Working preset 必须得到与 baseline 同量级数据。

---

# 37. 给 Codex 的第一条命令

```text
先不要修改任何文件。

请阅读：
1. 当前 main.cpp
2. platformio.ini
3. baseline_working.cpp（如果已有）
4. RF430_GUI_SPEC_for_Codex.md

先完成：
1. 总结当前已验证通信与测量流程；
2. 检查寄存器映射与现有代码是否一致；
3. 给出类和文件架构；
4. 给出 GUI 页面设计；
5. 给出 REST/WebSocket API；
6. 给出开发阶段；
7. 标出仍需查 TI 官方文档确认的项目。

任何无法从 TI 官方文档确认的寄存器行为，都标记为“待确认”，禁止猜测。

在我确认方案前，不要开始大规模重构。
```

---

# 38. 给 Codex 的第二条命令

```text
开始 Phase 2 和 Phase 3。

要求：
1. baseline_working.cpp 不动；
2. 封装 PN5180 + RF430 Block 读写；
3. 实现 rf430_registers.h；
4. 实现 ADC config encoder/decoder；
5. 实现 Block 0 / Block 2 builder；
6. 暂时不要做 Web GUI；
7. 保留串口 regression test。

验收：
- PlatformIO Build 通过；
- UID 仍可读取；
- Block 0/2/9 仍可读取；
- 当前 0x18 配置仍可采样；
- 电阻结果与 baseline 同量级；
- ×1+CIC256+SVSS = 0x18；
- ×2+CIC256+SVSS = 0x19。

完成后列出修改文件和测试结果。
```

---

# 39. 官方资料

## SLAU603B

`RF430FRL15xH Firmware User's Guide`

重点：
- 2.4 Possible Sensors
- 2.5 Sampling Configuration
- 2.6 Sensor Configuration
- 2.7 Thermistor
- 2.8 ADC Raw
- 2.9 Data Storage
- 2.11 FRAM Map
- 7.2～7.9 Block 0 registers
- 7.18～7.21 ADC config
- 7.25 Custom Timer
- 7.51 Logging FRAM

URL:

```text
https://www.ti.com/lit/ug/slau603b/slau603b.pdf
```

## SLAS834C

`RF430FRL15xH NFC ISO 15693 Sensor Transponder Datasheet`

重点：
- Pin assignments
- SVSS Generator
- Thermistor Bias
- ADC
- Application Circuit
- BOM

URL:

```text
https://www.ti.com/lit/ds/symlink/rf430frl152h.pdf
```

## SLOA212A

`Battery-Less NFC/RFID Temperature Sensing Patch`

用于无源 RF430 实际应用结构参考。

TI 页面：

```text
https://www.ti.com/tool/TIDM-RF430-TEMPSENSE
```

---

# 40. 最终目标

不是只做一个应变片 Demo，而是：

```text
RF430FRL152H 通用模拟 / 电阻传感器调试工具
```

支持：

```text
ADC1
ADC2
ADC0
Internal
Gain
Filter
Oversampling
Reference
Sampling Schedule
Averaging
Resistance
Baseline
ΔR
Strain
Raw Block
Live Plot
CSV
```

第一优先级：

```text
绝不破坏当前已经验证成功的测量链路。
```
