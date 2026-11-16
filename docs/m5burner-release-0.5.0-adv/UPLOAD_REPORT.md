# M5Burner Cardputer-ADV 0.5.0 公开提交记录

2026-11-16 已成功提交。后台确认：CardputerDict 中英离线词典 ADV，v0.5.0，待审核 / 公开，适配设备 Cardputer ADV。审核通过前不应视为已经上架。

后台：https://burner.m5stack.com/developer

原普通 Cardputer 条目仍为0.4.2；由于适配设备属于条目公共资料，本次为ADV新建独立条目。

## 上传产物

- CardputerDict-0.5.0-ADV-M5Burner.bin：2,141,568字节，ESP32-S3、8MB、完整合并镜像，烧录地址0x0。
- SHA-256：21b7a88e7e58f18674184a58193d30f2722ddd690b4803d9b403b63ec0796d14。与两款设备实测使用的0.5.0通用镜像相同。
- NVS区域0x9000–0xdfff均为0xff，上传的是构建产物，未上传设备备份或WiFi凭据；不含第三方发音录音。
- GIF封面：CardputerDict-0.5.0-ADV-UI-cover.gif，960×540，20帧，17.3秒，循环播放；每帧仅一个完整UI，4倍最近邻放大。
- 所有帧均从连接的Cardputer-Adv实际画布按操作顺序重新截取：逐字输入apple→Enter英汉释义→Space翻页→Tab切汉英→逐字输入pingguo→1选择苹果→Enter汉英释义→Space翻页→Tab回英汉。
- workflow.json逐帧记录命令、状态与展示时长；每帧解码后与原始画布4倍放大逐像素比对通过。
- 网站明确提示GIF未编辑时保留动画，已使用原GIF且未执行网站图片编辑器；通过系统文件选择器上传成功。
- submitted.png为提交后后台截图，显示待审核/公开及ADV设备。

完整词库需单独安装到FAT32 microSD，简介与版本说明已写明。全量合并烧录会重置片内WiFi/音量，SD文件保留，也已写明。

源码工程保留在 /Users/huxunxun/WorkSpace/CardputerDict；发布说明见public-description.md、release-notes.md，许可见licenses/。
