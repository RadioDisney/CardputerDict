# CardputerDict 构建与实机验证

日期：2026-11-10（Asia/Shanghai）

项目目录：`/Users/huxunxun/WorkSpace/CardputerDict`

## 完成状态

- 已创建完整 PlatformIO / Arduino 项目，编译并烧录到普通 Cardputer。
- 芯片：ESP32-S3 QFN56 rev v0.2，8 MB Flash；实机识别为 Cardputer。
- 已通过 USB 把演示词库、ECDICT 和许可证写入机内 SD；每个文件先校验传输内容，再回读 SD 校验，最后重命名。已有不同内容文件不会覆盖。
- 设备确认：`sd=1 words=770611 source=ECDICT 英汉词典`，可用堆内存 195,688 字节。
- 查询 apple 返回苹果释义和复数 apples；不存在的词返回 NOT_FOUND；拼音 ping 包含“苹”。
- 已读取真实帧缓冲区并目视检查，中文、音标 æ、候选栏显示正常。实体键盘完整操作仍建议用户按 README 逐项体验。

## 构建

PlatformIO espressif32 6.12.0，Arduino ESP32 2.0.17，M5Cardputer 1.1.1，M5Unified 0.3.0，M5GFX 0.3.0，IRremote 4.7.1。

最终静态 RAM：24,752 字节；程序 Flash：1,427,541 字节，应用分区 6 MB。

合并固件 CardputerDict-factory.bin 写入地址为 0x0；构建目录 firmware.bin 为应用分区，地址 0x10000。设备已经烧录，无需再次写入。

## 词库与内存

ECDICT：770,611 词头，正文 29,992,192 字节，索引 17,035,288 字节；稀疏索引占 48,164 字节 RAM。启动会扫描并校验完整索引，请等待屏幕进度完成。

中文拼音：413 个音节、6,763 个 GB2312 汉字，逐字选字。正文使用 efontCN_14 和从 DejaVu Sans 生成的 819 字形 Latin / IPA 后备字体。

ECDICT 是英文词头的英汉词库，不能直接用中文反向检索；demo 含少量汉英词头，可用于试验中文输入。

## 验证记录

主机测试采用 AddressSanitizer / UndefinedBehaviorSanitizer，每轮完成 10,000 次对照查询、重复词头跨索引边界、损坏数据、UTF-8 删除与截断，并验证实际 770,611 词的完整词库。Python 工具测试 3 项通过。日志在 docs/validation/。

最终设备状态：

```text
CardputerDict 0.1.0 board=14 heap=195688 sd=1 words=770611 source=ECDICT 英汉词典
note=
```

## 原固件备份

`backups/cardputer-before-20261110.bin` 为原机完整 8 MB Flash，已通过 esptool verify_flash 摘要比对。SHA-256：`0dae80d26219dbb419c3dd28a8457a6c5f492a87c803795930f324fc2993566a`。备份未放入可分享的输出包。恢复步骤见 docs/RECOVERY.md。

## 已知限制

USB 热复位可能使当前 SD 卡暂时不响应；已加入初始化重试、降速回退，并修正串口工具以避免切换复位控制线。若仍回退演示，Fn+R 重载；必要时断开 USB 并关机 5 秒。用户断电重启后，完整词库和最终字体检查已通过。

当前仅支持 UTF-8、未压缩、纯文本 m、32 位偏移的 StarDict；不支持 HTML、dict.dz、syn 别名、模糊查询、自动词形还原和词组拼音。单条释义最多显示 12 KB。

## 屏幕方向修正

初始化使用 `setRotation(0)`，配合当前 M5GFX 内置的 Cardputer 面板旋转偏移，得到 240×135 横屏；画布使用实际屏幕尺寸创建。重新编译和 USB 烧录通过，Flash 写入摘要校验成功；日志见 `docs/validation/rotation-upload.log`。合并固件和固件压缩包已同步更新。
