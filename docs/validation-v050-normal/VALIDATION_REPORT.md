# 普通 Cardputer 兼容性验证 — 0.5.0

2026-11-13（Asia/Shanghai）。目标 MAC `c0:4e:30:13:12:70`，USB `/dev/cu.usbmodem414401`。

使用与 Cardputer-Adv 完全相同的通用合并固件，SHA-256 `21b7a88e7e58f18674184a58193d30f2722ddd690b4803d9b403b63ec0796d14`，未改动固件源码。

## 已通过

- 自动识别 `DEVICE model=Cardputer board_id=14`，选择 `gpio-matrix` / `NS4168`。TCA8418、ES8311 探测为0，符合普通版硬件。
- 显示为240×135，rotation=0。SD 10MHz，容量480 MiB；复用现有数据和缓存，无需格式化或重新安装。
- 英汉770611词、汉英198650词头及拼音85610代码；简繁查询、整词拼音、紧凑候选分页与选词均通过。
- MP3 apple 和 WAV hello，重复播放、停止及音频后词库恢复通过；正常 PCM 帧数与峰值。
- 三次软件重启均为1420ms（主机观察1432–1433ms）。恢复配置后启动1453ms。
- 8ms按住/4ms间隔的原始扫描模拟：hello、letterll、bookkeeper、mississippi、eeeeeeee 全部正确。长按退格1200ms删除7次，松手停止；首尾循环翻页通过。
- 所有自动化检查均通过，键盘事件丢失为0。

## 备份与配置

刷写前保存完整8MiB备份并通过芯片摘要验证：`backups/cardputer-c04e30131270-before-v050-20261113.bin`。SHA-256 `8b6f4c2d8acc4561577d31fc89128a03addc977771fd0a25e777a30e26cf4847`。备份仅保存在主工程，不包含在分发包。

本次完整合并镜像刷写覆盖了 NVS（0x9000–0xdfff），随后已从本次备份恢复原分区，写入摘要验证通过。最终 `wifi_configured=1`、`volume=223`，与刷写前一致；密码未在聊天或报告中读取或展示。在线发音本次未测试。

现有设备升级优先使用 `CardputerDict-0.5.0-app.bin` 写入0x10000，或PlatformIO upload，以保留NVS；完整镜像写入0x0用于新机部署，会重置保存的设置。

## 实体确认

用户已于2026-11-13确认：“声音、方向和按键都正常”。实体连续同键输入、长按退格、Tab切换、拼音选词与查词、屏幕方向和朗读声音均已确认。最终只读观察到用户成功查询“苹果”，未丢键盘事件，原WiFi和音量配置正常。

证据见 `validation/normal/`；主工程中为 `docs/validation-v050-normal/`。
