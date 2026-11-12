# M5Burner 公开上传记录

2026-11-11 已上传 CardputerDict 中英离线词典 v0.3.0，并选择公开提交审核。页面当前显示“待审核 / 公开”，尚不能视为已通过审核上架。

- 设备：普通 Cardputer；分类：电子书与阅读。
- 上传文件：CardputerDict-0.3.0-M5Burner.bin。
- Flash：ESP32-S3，8 MB，烧录偏移 0x0。
- 包含 bootloader@0x0、分区表@0x8000、初始化数据@0xe000、应用@0x10000；各段与 PlatformIO 构建产物逐字节一致。
- 芯片标识为 ESP32-S3，bootloader 与应用 image_info 校验和/哈希有效，分区布局在 8 MB 范围内。
- 源码提交：ceefab088b8fccb9f9c390f8e75d79cc714d13f1，构建前工作区干净，PlatformIO 构建成功。
- SHA-256：f203c82b3bc9ef2a1e1c6d3656fed96cda1c092f4f3f1979249a281bcfb8e54b。
- 头图使用用户提供的 IMG_20261111_182159.jpg，保持完整画面，缩放为 1600×1202 JPEG，598,491 字节；发布页卡片已显示该照片。
- 已上传功能描述、版本说明、GitHub 源码链接、SD 词库安装方法和开源数据许可说明。

M5Burner Developer：https://burner.m5stack.com/developer
项目：https://github.com/RadioDisney/CardputerDict
规范核对：当前网页上传表单仅接受单个 .bin；已核对官方发布指南 https://docs.m5stack.com/en/uiflow/m5burner/publish 的名称、版本、描述、设备、GitHub、固件与封面字段。

完整词库仍需单独复制到 FAT32 microSD；固件内只有 29 词演示，发布说明已明确写明。

submitted.png 为提交成功后当前账号页面截图；description.md 与 release-notes.md 为提交的正文和版本说明。
