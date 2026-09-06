# RF430FRL152H Web Measurement Tool

ESP32-C3 + PN5180 + RF430FRL152H 的浏览器调试和电阻/应变测量工具。

## 已冻结基线

原始已验证程序保存在 `backup/baseline_working.cpp`，不得修改。基准配置：ADC1=Sensor、ADC2=200 kΩ Reference、ADC config=`0x18`（×1/CIC256/SVSS），实测传感器约 101.2–101.4 kΩ。

## 使用

1. `pio run -t upload`
2. 串口 115200 查看硬件状态。
3. 连接 Wi-Fi `RF430-Tool`，浏览器打开 `http://192.168.4.1`。
4. 页面修改设置后先 Save，再 Apply；开机只从 NVS 加载，不自动写 RF430。

## 无 Wi-Fi 的 USB 仪表盘

当前硬件可使用 `serial-dashboard` 环境，通过 USB 串口显示实时窗口，不启用 ESP32 Wi-Fi：

```bash
pio run -e serial-dashboard -t upload
```

烧录后双击项目根目录的 `启动RF430仪表盘.bat`。窗口提供连续/单次采样、停止、设置 R0、参考电阻、Gauge Factor、实时曲线和 CSV 导出。完整 Web 固件仍保留在 `esp32-c3-supermini` 环境。

提供 Device 状态、ADC 合法配置、Block 预览、单次/连续采样、R0、ΔR、ΔR/R0、Gauge Factor、应变 με、SSE 实时曲线、CSV、只读 Block 0–15 和 Current Working preset。Raw FRAM Write 未开放。

## 安全行为

非法 Filter/Rate、Reference=0、0/0x3FFF、Infinite+passes≠2 均拒绝；采样错误不会读取旧 Block 9。硬件操作由主循环异步轮询，RF430 离场时 Web 服务仍继续响应。
