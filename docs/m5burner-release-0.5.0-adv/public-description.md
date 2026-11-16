## CardputerDict 中英离线词典 — Cardputer-ADV

适用于 M5Stack Cardputer-ADV（ESP32-S3、8MB Flash）。同一份 0.5.0 固件也兼容普通 Cardputer，启动自动识别机型，选择键盘和音频驱动。两款设备均已实机验证，声音、显示方向和实体按键正常。

### 查词、拼音与朗读
- 离线英汉 770,611 个词头、汉英 198,650 个简繁体词头，85,610 种整词拼音组合。
- Tab 切换英汉/汉英；输入 pingguo → 苹果、nihao → 你好；v 代表 ü。空格或数字选择单行紧凑候选，[ / ] 翻候选页，Enter 打开词条。
- 支持中文和 IPA 音标；释义首尾循环翻页。Space/Enter 下一页，Fn+; 或 Fn+, 上一页，Fn+. 或 Fn+/ 下一页。
- 退格按住约450ms后，每125ms连续删除，松手停止；Fn+Backspace 清空。Fn+H / G0 帮助，Fn+D 切换词库。
- Fn+P 朗读英文词，再按或反引号停止；Fn+= / Fn+- 调音量。优先播放 audio/en 的 MP3/WAV，再使用 audio/cache/en 缓存。
- Fn+W 在设备设置2.4GHz WiFi；缺少本地音频时经验证 HTTPS 从 dict.youdao.com 获取所选英文词的发音并缓存，结束后关闭 WiFi。第三方接口可用性可能变化。固件不包含第三方录音包。

### Cardputer-ADV 适配
自动使用 TCA8418 键盘事件 FIFO、硬件去抖和 ES8311 音频；普通版使用 GPIO 矩阵与 NS4168。屏幕240×135横屏。SD 10/4/1MHz重试并补齐目录，不自动格式化。SD 索引首次建立后序列化缓存，实测 Adv 软件重启约1.61秒、普通版约1.42秒（随卡而异）。

### 首次使用：准备 FAT32 microSD
.bin 内仅有29词演示，完整词库须另行安装。从以下项目下载 ZIP，将 sdcard/ 中的 dictionaries/、licenses/ 复制到卡根目录；关机插卡再开机，首次索引耗时较长，之后从缓存启动。已有同格式词库及缓存可直接复用。

- [项目与使用说明](https://github.com/RadioDisney/CardputerDict)
- [SD词库目录](https://github.com/RadioDisney/CardputerDict/tree/master/sdcard)

Fn+R 重新挂载并加载缓存；Fn+Shift+R 强制校验重建。朗读可导入个人 MP3/WAV，支持8/16-bit PCM WAV，文件不超过512KB / 30秒。

### 烧录
M5Burner 上传文件为完整合并固件，ESP32-S3 / 8MB Flash，写入地址0x0。全量烧录会重置片内 WiFi 和音量设置，需重新配置；SD 文件保留。请先备份原固件。后续保留配置的应用升级可用项目的应用固件写入0x10000。

### 许可
应用原创代码 MIT；ECDICT MIT；MDBG / CC-CEDICT 派生汉英与词组拼音数据 CC BY-SA 4.0；mozillazg/pinyin-data MIT。字体、dr_mp3 和依赖保留各自许可，见 third_party/、sdcard/licenses/。支持未压缩UTF-8纯文本StarDict；生僻字受字体覆盖限制，无整句联想或自动词形还原。
