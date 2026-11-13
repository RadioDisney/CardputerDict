# Cardputer / Cardputer-Adv 通用固件 0.5.0

2026-11-13。使用官方 M5GFX / M5Unified 的板型检测，启动后选择设备配置。

- 普通版：GPIO 矩阵扫描、软件 2 ms 去抖、NS4168 音频。
- Adv：TCA8418（内部 I2C 0x34），完整排空事件 FIFO，采用硬件去抖，避免快按和同键连按被最终状态吞掉；ES8311（0x18）由 M5Unified 配置。
- 56 键逻辑映射一致；独立扫描任务、450 ms 退格延迟 / 125 ms 重复保持原行为。
- 屏幕保持 rotation=0，240×135；按设备识别结果初始化键盘。
- 两款 SD 均为 SCK40 / MISO39 / MOSI14 / CS12。重新挂载时重试 10 / 4 / 1 MHz，补齐词库、许可证和音频目录，不格式化。
- 兼容既有 StarDict 数据与 `.cidx` 缓存；无需为 Adv 重新转换词库。

## 构建与使用

`python3 -m platformio run -e cardputer` 构建同一份通用应用。`firmware.bin` 位于 0x10000；合并的 `CardputerDict-0.5.0-merged.bin` 位于 0x0。

首次安装 SD 完整词库后需生成索引缓存，后续启动直接加载。若 SD 提示未就绪，请确认 FAT32 文件系统及接触情况；此固件不会自动擦除卡。

新设备 WiFi 配置独立，可使用 Fn+W 在设备输入 2.4GHz WiFi，Fn+P 朗读。Adv 插入耳机时由硬件切换音频输出，测试扬声器时应拔下耳机。

## 实机验证

目标 MAC `70:af:09:df:1d:c0`，USB `/dev/cu.usbmodem414301`。已在刷写前保存并通过芯片摘要校验 8 MiB 完整备份。Adv 初次验证后，已接入普通 Cardputer 使用同一二进制完成自动化实机兼容验证；详见 `NORMAL_VALIDATION_REPORT.md`（源工程中为 `docs/validation-v050-normal/`）。

详细测试结果见同目录 `validation-v050/`。
