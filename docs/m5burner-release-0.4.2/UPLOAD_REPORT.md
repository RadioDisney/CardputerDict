# M5Burner v0.4.2 上传记录

2026-11-12 已作为现有 CardputerDict 中英离线词典的新版本公开提交。版本列表确认 v0.4.2 为“待审核 / 公开”，v0.3.0 继续“已发布 / 公开”。不能把待审核视为已上架。

版本页面：https://burner.m5stack.com/developer/firmware/2120468585455984642/versions

公共资料已补充 0.4.x 英语朗读说明、Fn+P/Fn+W、125ms 退格重复和循环翻页，保存后页面显示“资料审核中”；审批前访客可能仍看到旧的公共描述。新版版本说明已完整介绍当前功能。

## 上传内容

- 版本：0.4.2；设备：普通 Cardputer；公开提交审核。
- 源码：e77918149e850b87c9912632df98276839c7eece，上传前工作区干净，PlatformIO 构建成功。
- 固件：CardputerDict-0.4.2-M5Burner.bin，2,138,816 字节，ESP32-S3 / 8MB Flash，偏移 0x0。
- 包含 bootloader@0x0、分区表@0x8000、初始化数据@0xe000、应用@0x10000；逐段比对构建产物通过。
- SHA-256：3ca64b058f471cb7674408b321ae6e8c7861c4ca2f97b93bd95f08a17ca3d60b。
- 检查 NVS 区域全部为擦除填充值，无设备 WiFi 凭据；使用构建合并文件，不是设备 Flash 导出。
- 新封面：CardputerDict-0.4.2-UI-cover.png，1600×1160，由当前设备的英汉释义、汉英释义、整词拼音和朗读界面 4 张截图拼成。截图按 3 倍最近邻放大；未包含 WiFi 设置或凭据。
- 版本说明保存为 release-notes.md，公共介绍保存为 public-description.md，提交成功截图为 submitted.png。

完整词库仍需单独复制到 FAT32 microSD，版本说明明确记录；此上传不包含第三方发音录音，音频按需获取/导入。全量合并固件烧录重置片内设置，SD 文件保留。

本地 bundle 内含上传二进制、封面和截图源、说明、校验摘要与许可证；M5Burner 表单实际接收的是单个合并 .bin 和 UI 封面图。
