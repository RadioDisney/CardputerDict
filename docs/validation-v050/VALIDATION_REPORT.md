# CardputerDict 0.5.0 Adv 移植验证

日期：2026-11-13（Asia/Shanghai）。

## 实现与部署

同一份固件启动时自动识别 Cardputer / Cardputer-Adv。普通版保留 GPIO 矩阵扫描与 NS4168 音频；Adv 使用 TCA8418 硬件 FIFO 和 ES8311。共享 SD 引脚，重新挂载并补齐目录，未格式化。主工程为 `/Users/huxunxun/WorkSpace/CardputerDict`。

最终固件 SHA-256：`21b7a88e7e58f18674184a58193d30f2722ddd690b4803d9b403b63ec0796d14`，合并镜像地址 `0x0`，应用地址 `0x10000`。

原始 8MiB 备份：`backups/cardputer-adv-70af09df1dc0-before-20261113.bin`，SHA-256 `a584a7ee9457de920b6b27bb9a8f14e5467e045afe74cc8ec5792846b154afa0`，已通过 esptool verify_flash 芯片摘要比对。备份只保存在主工程，不包含在公开分发包中。

## 验证结果

- 构建：PlatformIO 成功，RAM 66108/327680 字节，应用 2075661/6291456 字节。
- 主机测试：8 个原生测试程序，11 个 Python 用例；ASan/UBSan；真实三套词库和索引缓存验证全部通过。
- 实机：MAC `70:af:09:df:1d:c0`，board_id=24，TCA8418=1，ES8311=1。另一 USB 串口未刷写。
- SD：480 MiB，10MHz，sd=1，dirs_ready=1。安装文件逐一 FNV1a 回读校验；已有相同文件跳过，不同文件保留。
- 数据：ECDICT 770611 词，CC-CEDICT 198650 词头，拼音 85610 代码。简繁汉英查词及拼音词组选择通过。
- 首次缓存：ECDICT 66210 ms、汉英 11519 ms、拼音 5095 ms；后续缓存加载约 396 / 332 / 296 ms。
- 3 次重启：均在固件计时 1611 ms 完成，主机观察 1623–1624 ms。
- 屏幕：240×135、rotation=0；紧凑单行候选、音标及循环翻页通过，并保存 framebuffer 截图。
- 输入：模拟原始扫描输入 hello、letterll、bookkeeper、mississippi、eeeeeeee 全部一致；1200ms 退格按住删除7次，松手停止。用户已确认实体连续同键输入、长按退格、Tab 切换、拼音输入与查词均正常。
- 音频：MP3 apple=56208 PCM帧 / 峰值24918，WAV hello=60096帧 / 峰值25265；重复播放、停止、词库恢复通过；测试期间 I2C 错误、键盘队列丢失与 FIFO 溢出均为0。用户已确认 apple / hello 实际朗读声音正常。
- 新 Adv 未配置 WiFi；本次验证为离线音频路径，在线发音可通过 Fn+W 配置后使用。
- 初次 Adv 移植后，又在普通 Cardputer 上刷入同一镜像完成实机自动化兼容验证；详见 `NORMAL_VALIDATION_REPORT.md`。

## 操作确认

用户已于 2026-11-13 确认：“声音、方向和按键都正常”。Cardputer-Adv 实体按键、屏幕方向与扬声器验证完成。

测试证据见 `validation/`（源工程中为 `docs/validation-v050/`）。
