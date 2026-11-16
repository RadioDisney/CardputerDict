## 0.5.0：Cardputer-ADV 兼容与自动识别

- 同一固件启动自动识别普通 Cardputer / Cardputer-ADV；本条目设备选择 Cardputer-ADV。
- Adv 采用 TCA8418 硬件去抖并排空按键 FIFO，保留快速连按；音频使用 ES8311。普通版继续使用 GPIO 矩阵与 NS4168。
- SD 引脚兼容，10/4/1MHz挂载重试并补齐目录，不格式化；沿用StarDict词库和.cidx缓存。
- 两款设备都完成英汉/汉英、拼音、候选布局、音标、循环翻页、MP3/WAV、连续同键与退格连发测试；用户确认声音、显示方向和实体按键均正常。
- Adv 三次软件重启均约1.61秒；普通版约1.42秒。实际速度取决于SD卡。
- 保留半角1px/全角2px字距、释义4px留白、单行紧凑候选、125ms退格重复及WiFi发音缓存。
- 新封面为按实际查词顺序截取的20帧GIF：输入apple→打开英文释义并翻页→Tab切换汉英→逐字输入pingguo→按1选择苹果→Enter打开汉英释义并翻页→Tab返回英汉。每帧仅一个完整UI，960×540整数放大，无拼图。

### 安装须知
完整合并.bin写入0x0，ESP32-S3 / 8MB；全量烧录重置WiFi及音量，SD数据保留。固件仅内置29词演示，完整词库请下载项目ZIP，将sdcard/内容复制到FAT32卡根目录。已有词库无需重复复制，首次缓存建立后快速启动。

[项目与SD词库](https://github.com/RadioDisney/CardputerDict)

固件为构建产物，无用户WiFi凭据；不包含第三方录音包。应用MIT，ECDICT MIT，CC-CEDICT派生数据CC BY-SA 4.0；其余依赖见项目许可证。
