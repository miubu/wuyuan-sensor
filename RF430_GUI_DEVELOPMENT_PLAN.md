# RF430FRL152H Web GUI 项目开发计划

> 项目目标：在已经验证成功的 **ESP32-C3 + PN5180 + RF430FRL152H** 电阻测量程序基础上，开发一个可通过浏览器配置、采样、显示、记录和导出数据的图形化调试工具。  
> 本计划供 Codex 按阶段实施，原则是：**先保证当前功能不被破坏，再逐层增加配置、测量、Web API 和 GUI。**

---

# 1. 项目目标

最终程序应成为一个：

```text
RF430FRL152H 通用传感器调试与测量工具
```

支持：

- PN5180 / RF430 状态检测
- ISO15693 读写
- ADC1 / ADC2 / ADC0 配置
- 增益配置
- CIC / Moving Average 滤波
- Oversampling / Decimation 配置
- SVSS / AVSS 选择
- RF430 ROM Sensor Firmware 参数配置
- ADC RAW 数据读取
- 电阻计算
- R0 基准
- ΔR
- ΔR/R0
- Gauge Factor
- 应变 με
- 连续采样
- 实时曲线
- CSV 导出
- Raw Block 调试
- 参数持久化
- 浏览器 GUI

---

# 2. 固定硬件条件

## 2.1 ESP32-C3 SuperMini 与 PN5180

固定引脚：

```cpp
#define PN5180_NSS   3
#define PN5180_BUSY  5
#define PN5180_RST   4

#define PN5180_SCK   6
#define PN5180_MISO  7
#define PN5180_MOSI  10
```

禁止 Codex 擅自修改这些引脚。

---

## 2.2 当前 RF430 接线

当前实际硬件：

```text
ADC1 → 应变/压阻传感器 → SVSS
ADC2 → 200 kΩ参考电阻 → SVSS

SVSS → 1 µF → VSS/GND
```

当前默认逻辑角色：

```text
ADC1 = Sensor
ADC2 = Reference
```

当前参考电阻：

```text
Rref = 200000 Ω
```

---

# 3. 当前已经验证成功的基准

必须保留当前成功代码作为：

```text
baseline_working.cpp
```

禁止后续重构覆盖。

当前已经实际验证：

```text
RF430 UID:
E0:07:A2:00:00:0A:DF:8D

Block size:
8 bytes

Block count:
243
```

当前成功 ADC 配置：

```text
ADC1 = 0x18
ADC2 = 0x18
```

对应：

```text
Gain = ×1
Filter = CIC
Decimation = 256
Reference = SVSS
```

当前测量实例：

```text
ADC1 Sensor RAW = 5320
ADC2 Reference RAW = 10498

Rref = 200000 Ω

Rsensor =
200000 × 5320 / 10498
≈ 101352.64 Ω
```

第二次：

```text
ADC1 = 5314
ADC2 = 10500

Rsensor ≈ 101219 Ω
```

这组值应作为后续 GUI 版本的回归测试基准。

---

# 4. 总开发原则

Codex 必须遵守：

1. **不先重写底层 PN5180 通信。**
2. **先备份当前工作版本。**
3. **每个阶段都必须可以编译。**
4. **每个阶段完成后都要跑回归测试。**
5. RF430 寄存器位必须依据 TI 官方文档。
6. 不允许凭经验猜寄存器。
7. 不允许大量散落 `0x18`、`0x40` 等 magic numbers。
8. Web GUI 开发放在底层稳定之后。
9. RF430 离开磁场不能导致 ESP32 卡死。
10. PN5180 BUSY 等待必须增加 timeout。
11. ISO15693 读写错误不能导致死循环。
12. Raw FRAM Write 默认关闭。
13. 当前成功的 200 kΩ 电阻测量必须始终可以恢复。

---

# 5. 推荐最终目录结构

```text
RF430_GUI/
│
├── platformio.ini
│
├── README.md
│
├── RF430_GUI_SPEC_for_Codex.md
│
├── RF430_GUI_DEVELOPMENT_PLAN.md
│
├── docs/
│   ├── SLAU603B_RF430FRL15xH_Firmware_Users_Guide.pdf
│   ├── RF430FRL152H_Datasheet.pdf
│   └── SLOA212A.pdf
│
├── src/
│   ├── main.cpp
│   ├── baseline_working.cpp
│   │
│   ├── pn5180_transport.h
│   ├── pn5180_transport.cpp
│   │
│   ├── rf430_registers.h
│   ├── rf430_config.h
│   ├── rf430_config.cpp
│   │
│   ├── rf430_device.h
│   ├── rf430_device.cpp
│   │
│   ├── measurement.h
│   ├── measurement.cpp
│   │
│   ├── settings.h
│   ├── settings.cpp
│   │
│   ├── web_api.h
│   └── web_api.cpp
│
└── data/
    ├── index.html
    ├── app.js
    └── style.css
```

---

# 6. Phase 0：冻结当前工作版本

## 目标

确保任何后续修改失败都可以恢复。

## Codex 任务

1. 找到当前成功的 `src/main.cpp`
2. 原样复制为：

```text
src/baseline_working.cpp
```

或者保存到：

```text
backup/baseline_working.cpp
```

3. 不修改这份文件。

## 必须记录

在 README 中写明当前基准：

```text
ADC1 = Sensor
ADC2 = 200k Reference
ADC Config = 0x18
Measured R ≈ 101.2–101.4 kΩ
```

## 验收条件

- baseline 文件存在
- 与原代码逐字一致
- PlatformIO 原程序仍可编译

---

# 7. Phase 1：代码与 TI 文档审计

## 目标

先理解，不写功能。

## Codex 必须阅读

```text
1. 当前 main.cpp
2. baseline_working.cpp
3. platformio.ini
4. RF430_GUI_SPEC_for_Codex.md
5. SLAU603B
6. RF430FRL152H Datasheet
7. SLOA212A
```

## 输出内容

Codex 第一轮只输出分析：

### 7.1 当前代码调用链

例如：

```text
setup
↓
SPI init
↓
PN5180 setupRF
↓
Inventory
↓
getSystemInfo
↓
write Block 2
↓
write Block 0
↓
poll status
↓
read Block 9
↓
decode ADC1 / ADC2
↓
calculate resistance
```

### 7.2 寄存器映射

必须核对：

```text
Block 0
Block 1
Block 2
Block 9
```

### 7.3 风险清单

例如：

- PN5180 reset 阻塞
- BUSY 无限等待
- RF430 离场
- 旧 Status 被误判为新数据
- ADC saturation
- reference=0
- FRAM 写入错误
- Web 服务被硬件阻塞

## 验收条件

这个阶段：

```text
不修改任何源代码
```

---

# 8. Phase 2：封装 PN5180 Transport

## 目标

把 PN5180 底层通信从业务逻辑中拆出来，但不改变行为。

## 新文件

```text
pn5180_transport.h
pn5180_transport.cpp
```

## 建议接口

```cpp
class PN5180Transport {
public:
    bool begin();
    bool reset();
    bool setupRF();

    bool inventory(uint8_t uid[8]);

    bool getSystemInfo(
        const uint8_t uid[8],
        uint8_t& blockSize,
        uint8_t& blockCount
    );

    bool readBlock(
        const uint8_t uid[8],
        uint8_t block,
        uint8_t* data,
        uint8_t size
    );

    bool writeBlock(
        const uint8_t uid[8],
        uint8_t block,
        const uint8_t* data,
        uint8_t size
    );

    int lastError() const;
};
```

## 必须增加

### 超时机制

所有潜在等待：

```text
BUSY
Reset
RF command
```

都不能无限等待。

### 错误返回

禁止：

```cpp
while(1);
```

建议：

```cpp
return false;
```

由上层恢复。

## 验收条件

必须仍能：

```text
识别 RF430
读取 UID
读取 Block 0
读取 Block 2
读取 Block 9
```

---

# 9. Phase 3：建立 RF430 寄存器模型

## 目标

消灭散落的 magic numbers。

## 新文件

```text
rf430_registers.h
```

## 内容

例如：

```cpp
namespace RF430 {

constexpr uint8_t BLOCK_CONTROL = 0;
constexpr uint8_t BLOCK_ADC_CONFIG = 2;
constexpr uint8_t BLOCK_DATA_START = 9;

constexpr uint8_t GENERAL_START =
    1 << 0;

constexpr uint8_t SENSOR_ADC1 =
    1 << 0;

constexpr uint8_t SENSOR_ADC2 =
    1 << 1;

constexpr uint8_t SENSOR_ADC0 =
    1 << 2;

constexpr uint8_t ERROR_USING_THERMISTOR =
    1 << 6;

}
```

## 增加 Enum

```cpp
enum class AdcGain {
    X1,
    X2,
    X4,
    X8
};

enum class AdcFilter {
    CIC,
    MovingAverage
};

enum class AdcReference {
    SVSS,
    AVSS
};
```

## 验收条件

业务代码中不应继续大量出现：

```text
0x18
0x19
0x2B
0x03
0x40
```

---

# 10. Phase 4：实现 ADC Config Encoder / Decoder

## 目标

用户选择参数，程序自动得到配置字。

## 新文件

```text
rf430_config.h
rf430_config.cpp
```

## 建议结构

```cpp
struct AdcConfig {
    AdcGain gain;
    AdcFilter filter;
    uint16_t rate;
    AdcReference reference;
};
```

## 编码函数

```cpp
uint8_t encodeAdcConfig(
    const AdcConfig& config
);
```

## 解码函数

```cpp
AdcConfig decodeAdcConfig(
    uint8_t value
);
```

## 必须进行的测试

### Test 1

```text
Gain ×1
CIC
256
SVSS
```

必须：

```text
0x18
```

### Test 2

```text
Gain ×2
CIC
256
SVSS
```

必须：

```text
0x19
```

### Test 3

GUI 选择非法组合时：

```text
拒绝
```

例如：

```text
Moving Average + CIC 2048 rate
```

不能被生成。

## 验收条件

Encoder 与 Decoder 互逆。

---

# 11. Phase 5：实现 RF430 Device 层

## 目标

把 Block 操作转化成 RF430 语义。

## 新文件

```text
rf430_device.h
rf430_device.cpp
```

## 建议接口

```cpp
class RF430Device {
public:
    bool detect();

    bool readSystemInfo();

    bool readControlBlock(
        uint8_t out[8]
    );

    bool readAdcConfigBlock(
        uint8_t out[8]
    );

    bool writeAdcConfig(
        ...
    );

    bool applyMeasurementConfig(
        ...
    );

    bool startSampling();

    bool getSamplingState(
        ...
    );

    bool readSampleData(
        ...
    );
};
```

## 注意

Block 0 中：

```text
Start
```

必须最后触发。

推荐流程：

```text
Build Block 2
↓
Write Block 2
↓
Read-back verify
↓
Build Block 0 without Start
↓
Apply control config
↓
Set Start
```

---

# 12. Phase 6：Measurement Service

## 目标

把“采样一次”变成一个完整服务。

## 新文件

```text
measurement.h
measurement.cpp
```

## 流程

```text
Check RF430
↓
Apply ADC config
↓
Start
↓
Wait state
↓
Read Block 9
↓
Decode enabled channels
↓
Validate raw
↓
Calculate resistance
↓
Calculate baseline / delta
↓
Output result
```

## 推荐数据结构

```cpp
struct MeasurementResult {
    uint32_t timestampMs;

    bool valid;

    uint16_t adc1Raw;
    uint16_t adc2Raw;
    uint16_t adc0Raw;

    double sensorResistance;
    double referenceResistance;

    double r0;
    double deltaR;
    double relativeDelta;

    double gaugeFactor;
    double strain;
};
```

---

# 13. Phase 7：电阻角色配置

## 目标

支持 ADC1 / ADC2 角色交换。

## 配置

```cpp
enum class ChannelRole {
    Ignore,
    Sensor,
    Reference
};
```

用户可以设置：

```text
ADC1 = Sensor
ADC2 = Reference
```

或者：

```text
ADC1 = Reference
ADC2 = Sensor
```

## 算法

如果：

```text
ADC1 Sensor
ADC2 Reference
```

则：

```cpp
R =
Rref *
ADC1 /
ADC2;
```

如果：

```text
ADC1 Reference
ADC2 Sensor
```

则：

```cpp
R =
Rref *
ADC2 /
ADC1;
```

## 异常判断

如果 reference：

```text
0
```

禁止计算。

如果任意测量通道：

```text
0x3FFF
```

显示：

```text
SATURATED
```

---

# 14. Phase 8：串口回归测试

在加 Web 之前必须先验证 Measurement Service。

输出至少：

```text
UID
ADC config
Block 0
Block 2
Block 9
ADC1
ADC2
Rref
Rsensor
```

### 当前 Working Preset

```text
ADC1 role = Sensor
ADC2 role = Reference

ADC1 config = ×1 CIC256 SVSS
ADC2 config = ×1 CIC256 SVSS

Rref = 200000 Ω
```

预期：

```text
ADC1 ≈ 5300
ADC2 ≈ 10500
Rsensor ≈ 101 kΩ
```

允许传感器实际漂移，但不能：

```text
0
0x3FFF
完全错误数量级
```

---

# 15. Phase 9：参数持久化

## 目标

ESP32 重启后保留 GUI 设置。

使用：

```cpp
Preferences
```

保存：

```text
ADC1 config
ADC2 config
ADC0 config

ADC1 role
ADC2 role

Rref
R0
Gauge Factor

Frequency
Passes
Averaging
Infinite sampling
```

## 重要安全要求

开机：

```text
只加载配置
```

不要：

```text
自动立即写 RF430
```

必须等：

```text
用户 Apply
```

---

# 16. Phase 10：Web Server 最小版本

## 目标

先做 API，不先做漂亮网页。

ESP32-C3 建议：

```text
Wi-Fi AP
SSID = RF430-Tool
```

第一版 API：

```text
GET /api/status
GET /api/config
POST /api/config
POST /api/sample
```

`/api/status` 示例：

```json
{
  "pn5180": true,
  "rf430": true,
  "uid": "E0:07:A2:00:00:0A:DF:8D",
  "block_size": 8,
  "block_count": 243,
  "state": "idle"
}
```

---

# 17. Phase 11：实时数据通道

## 目标

浏览器不刷新即可收到测量值。

推荐：

```text
WebSocket
```

或者：

```text
SSE
```

建议数据：

```json
{
  "timestamp_ms": 12345,
  "adc1_raw": 5320,
  "adc2_raw": 10498,
  "resistance_ohm": 101352.6,
  "delta_r_ohm": 85.2,
  "relative_delta": 0.00084,
  "strain_ue": 400.1
}
```

## 要求

RF430 离场时：

```json
{
  "rf430": false
}
```

WebSocket 不能因此断掉或整个 ESP32 卡住。

---

# 18. Phase 12：基础 Web GUI

## 页面 1：Device

显示：

```text
PN5180 status
RF430 status
UID
Block size
Block count
Sampling state
```

按钮：

```text
Scan
Reset
Refresh
```

---

## 页面 2：ADC Config

ADC1 / ADC2 / ADC0：

```text
Enable
Gain
Filter
Rate
Reference
```

选择：

```text
Gain:
×1
×2
×4
×8
```

Filter：

```text
CIC
Moving Average
```

CIC：

```text
32
64
128
256
512
1024
2048
```

Moving Average：

```text
4096
8192
16384
32768
```

显示：

```text
Encoded byte: 0x18
```

---

# 19. Phase 13：配置 Preview

## 目标

写 RF430 前必须预览。

显示：

```text
Block 0:
01 00 03 03 01 01 00 40

Block 2:
18 18 00 05 00 FF FF FF
```

按钮：

```text
Apply Configuration
```

流程：

```text
选择参数
↓
Preview
↓
Apply
↓
Write
↓
Read-back verify
```

---

# 20. Phase 14：Measurement GUI

显示：

```text
ADC1 RAW
ADC2 RAW
ADC0 RAW

Reference Resistance
Sensor Resistance

R0
ΔR
ΔR/R0

Gauge Factor
Strain
```

按钮：

```text
Single Sample
Start Continuous
Stop
Set R0
```

---

# 21. Phase 15：实时曲线

至少提供：

```text
Resistance vs time
ΔR/R0 vs time
```

可选：

```text
ADC1 RAW
ADC2 RAW
```

保存：

```text
500～1000 points
```

按钮：

```text
Start
Stop
Clear
```

避免无限增长浏览器内存。

---

# 22. Phase 16：R0 与应变

## Baseline

用户点击：

```text
Set Current as R0
```

保存：

```cpp
R0 = currentResistance;
```

## ΔR

```text
ΔR = R - R0
```

## Relative

```text
ΔR/R0
```

## Gauge Factor

用户输入：

```text
GF
```

## Strain

```text
ε =
(ΔR/R0) / GF
```

输出：

```text
με = ε × 1e6
```

---

# 23. Phase 17：CSV 导出

CSV 字段建议：

```text
timestamp_ms
adc1_raw
adc2_raw
adc0_raw
reference_ohm
sensor_ohm
r0_ohm
delta_r_ohm
relative_delta
gauge_factor
strain
strain_ue
```

下载文件例如：

```text
rf430_measurement_20260905.csv
```

---

# 24. Phase 18：Raw Block Debug

## 默认只读

支持：

```text
Block number
Read
Hex
```

并可一次显示：

```text
Block 0～15
```

## Raw Write

第一版建议：

```text
不做
```

如果后期做：

- 高危确认
- 地址限制
- write verify
- 不允许写敏感 FRAM 区域

---

# 25. Phase 19：Presets

至少：

```text
Current Working
Fast
Balanced
Low Noise
Custom
```

## Current Working

必须固定当前成功配置：

```text
ADC1 = Sensor
ADC2 = Reference
Rref = 200000 Ω

ADC1:
×1
CIC256
SVSS

ADC2:
×1
CIC256
SVSS

UsingThermistor = ON
```

其他 preset：

```text
参数 → encoder → byte
```

禁止直接在多个地方写 hex。

---

# 26. Phase 20：稳定性改造

必须测试：

## 26.1 RF430 离开

运行连续测量时把 RF430 拿走：

程序应该：

```text
RF430 missing
```

而不是：

```text
ESP32 死机
Web GUI 停止响应
```

---

## 26.2 RF430 重新进入

应自动恢复：

```text
scan
detect
resume
```

---

## 26.3 PN5180 异常

SPI / BUSY 超时：

```text
error
reset
retry
```

禁止永久阻塞。

---

# 27. Phase 21：性能测试

测试不同 ADC 配置：

```text
CIC32
CIC64
CIC128
CIC256
CIC512
CIC1024
CIC2048
```

记录：

```text
conversion time
noise
standard deviation
resistance stability
```

后续 GUI 可以显示：

```text
Estimated conversion time
```

帮助用户选择配置。

---

# 28. Phase 22：测量统计功能

连续采样增加：

```text
Mean
Standard deviation
Min
Max
Peak-to-peak
```

分别对：

```text
ADC RAW
Resistance
ΔR
```

统计。

---

# 29. Phase 23：最终 README

README 必须包含：

## 硬件

```text
ESP32-C3
PN5180
RF430FRL152H
pin mapping
```

## 编译

```bash
pio run
```

## 上传

```bash
pio run -t upload
```

## 串口

```bash
pio device monitor
```

## Web GUI

```text
Connect Wi-Fi:
RF430-Tool

Open:
ESP32 AP IP
```

## 默认配置

```text
ADC1 Sensor
ADC2 200k Reference
×1 CIC256 SVSS
```

---

# 30. 每阶段统一验收模板

Codex 每完成一个 Phase，都必须回复：

```text
### Completed
完成了什么

### Modified Files
修改了哪些文件

### Build
PlatformIO 是否通过

### Test
做了什么测试

### Regression
当前 RF430 电阻测量是否仍正常

### Known Issues
仍存在什么问题

### Next Step
下一阶段是什么
```

---

# 31. 推荐给 Codex 的第一条执行命令

```text
请严格按照 RF430_GUI_DEVELOPMENT_PLAN.md 执行。

现在只做 Phase 0 和 Phase 1。

要求：

1. 备份当前已验证成功的 main.cpp；
2. 不修改 baseline；
3. 阅读 RF430_GUI_SPEC_for_Codex.md；
4. 阅读 TI 官方资料；
5. 分析当前程序；
6. 输出寄存器映射检查；
7. 输出项目架构；
8. 输出风险；
9. 不开始 Web GUI；
10. 不进行大规模代码修改。

完成后按照计划中的统一验收模板汇报。
```

---

# 32. 第二条 Codex 命令

Phase 0/1 确认后：

```text
执行 Phase 2、Phase 3、Phase 4。

目标：

1. 封装 PN5180 Transport；
2. 创建 rf430_registers.h；
3. 创建 ADC Config encoder/decoder；
4. 保持现有串口测量功能；
5. 不做 Web GUI。

必须验证：

×1 + CIC256 + SVSS = 0x18
×2 + CIC256 + SVSS = 0x19

必须 PlatformIO Build 成功。

完成后报告修改文件和回归测试结果。
```

---

# 33. 第三条 Codex 命令

```text
执行 Phase 5～Phase 8。

目标：

1. RF430 Device abstraction；
2. Measurement Service；
3. ADC1 / ADC2 role 可交换；
4. 当前硬件默认：
   ADC1 Sensor
   ADC2 Reference
   Rref=200000Ω
5. 完成串口 regression test。

暂时不要开发 Web GUI。

验收：

ADC1/ADC2 RAW 有效；
电阻结果应与 baseline 同数量级；
不得出现 0 或 0x3FFF 而仍继续算电阻。
```

---

# 34. 第四条 Codex 命令

```text
执行 Phase 9～Phase 13。

实现：

Preferences
Wi-Fi AP
HTTP API
WebSocket
基础 Device 页面
ADC Config 页面
Configuration Preview

暂时不要做复杂曲线。

Web 服务必须在 RF430 不存在时仍然正常响应。
```

---

# 35. 第五条 Codex 命令

```text
执行 Phase 14～Phase 19。

实现：

Measurement GUI
Single Sample
Continuous Sample
R0
ΔR
ΔR/R0
Gauge Factor
strain με
Live Chart
CSV Export
Presets
Raw Block Read
```

---

# 36. 最终验收标准

项目只有同时满足以下条件才算完成。

## 硬件通信

- PN5180 正常初始化
- RF430 Inventory 正常
- UID 正常
- Block read/write 正常

## RF430 配置

- ADC Gain 可选
- Filter 可选
- Rate 可选
- SVSS/AVSS 可选
- Sensor enable 可选
- Passes 可配
- Averaging 可配
- Infinite sampling 可配
- Resistive Bias 可配

## 数据

- RAW 正确
- saturation 检测
- reference=0 检测
- 电阻计算正确
- 通道角色可交换

## GUI

- Web 页面正常
- 实时数据显示
- 曲线正常
- R0 正常
- ΔR 正常
- 应变计算正常
- CSV 可导出

## 稳定性

- RF430 离场不死机
- RF430 回来可恢复
- PN5180 错误可恢复
- Web GUI 不被硬件阻塞

## 回归

Current Working preset 下：

```text
ADC1 ≈ 5k counts
ADC2 ≈ 10.5k counts
Sensor ≈ 101 kΩ
```

与当前真实系统保持同数量级。

---

# 37. 最重要的一句话

整个开发过程中始终遵循：

```text
先保持已经成功的 RF430 测量链路，
再增加抽象，
再增加 API，
最后增加 GUI。
```

不要反过来先写一个漂亮界面，再回来修底层通信。
