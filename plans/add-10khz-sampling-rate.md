# 添加 10kHz 采样率选项

## 背景
当前逻辑分析仪支持 500kHz 和 1MHz 两种采样率。需要新增 10kHz 采样率用于采集 1kHz 及以下频率的信号。

## 约束条件
- TIM1 时钟：96MHz（APB2, DIV1）
- DMA 缓冲区：4096 个 uint16_t
- USB CDC 缓冲区：2048 字节
- 10kHz → ARR = 96000000/10000 - 1 = 9599（在 16 位范围内）

## 修改文件清单

### 1. `Core/Inc/DmaSampler.h`
- 添加 `#define LA_RATE_10K_HZ  10000U`

### 2. `Core/Inc/LogicAnalyzer.h`
- 在 `MODE` 枚举中添加 `LOGIC_ANALYZER_MODE_STREAM_10K = 3U`

### 3. `Core/Src/LogicAnalyzer.c`
- `LogicAnalyzer_ProcessByte()`：添加 `cmd == 4U` 分支，设置 `s_mode = LOGIC_ANALYZER_MODE_STREAM_10K`
- `LogicAnalyzer_ParseCommand()`：添加新命令映射（`'4'` / `"low"` / `"10k"` → 返回 4）
- `LogicAnalyzer_GetRateForMode()`：添加 `LOGIC_ANALYZER_MODE_STREAM_10K` → `LA_RATE_10K_HZ` 映射

### 4. `Core/Src/main.c` — 无需修改
`HandleModeChange()` 通过 `LogicAnalyzer_GetRateForMode()` 动态获取速率，自动适配。

## 验证
- 发送 `4\n` 或 `low\n` 或 `10k\n` 命令启动 10kHz 采样
- ARR 应为 9599，实际采样率 = 96MHz/9600 = 10kHz
- 半缓冲 2048 个采样点填满时间 = 2048/10000 = 0.2048 秒
- USB 数据率 ≈ 10 KB/s，远低于 USB FS 上限
