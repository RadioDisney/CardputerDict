## 0.4.2：英语朗读与键盘响应优化

### 新增英语朗读
- Fn+P 朗读当前英文词条，再按 Fn+P 或反引号停止。Fn+= / Fn+- 调节音量。
- 优先播放 SD 的 audio/en 本地 MP3/WAV；其次使用 audio/cache/en 缓存。缺少本地音频时，经已配置的 WiFi 获取并缓存，下次可离线播放。
- Fn+W 在设备上设置 2.4GHz WiFi；不需要把密码传给电脑。配置保留在设备 NVS。
- 联网时只把选中的英文词发送到有道 dict.youdao.com，使用 HTTPS 证书校验和 NTP 校时。不开机自动联网，获取完成或失败后关闭 WiFi。
- 支持 MP3 与 8/16-bit PCM WAV、单声道/双声道、8–48kHz，文件上限 512KB、30秒；流式解码。服务音频按需获取，固件和项目不附带第三方录音包。
- 可用 tools/prepare_audio_pack.py 准备个人常用词音频，或 tools/install_audio.py 经 USB 导入。

### 更顺手的输入与翻页
- 退格按住 450ms 后，每 125ms 连续删除，松手停止；中文按完整 UTF-8 字符删除。Fn+退格仍为单次清空。
- 独立 1ms 键盘扫描、2ms 去抖与事件队列，缩短相同字符连按需要的松手间隔，避免刷新时漏输 ll / ee。
- 释义首尾循环：末页下一页回首页，首页上一页到末页；Space、Enter 与 Fn 翻页键一致。

### 内存与稳定性
- 索引改为最多 4KB 一块的存储，SD 序列化缓存流式恢复，避免 WiFi 后的堆碎片影响查词。兼容 0.3.x 的 .cidx。
- 播放结束释放扬声器缓冲，自动恢复词典索引。已验证连续播放、停止、联网获取、断网缓存重播和重启后配置保留。
- 保留全局半角 1px/全角 2px 字距、释义 4px 行间留白、列表滚动和 [Tab] 提示。

### 安装须知
适配普通 Cardputer（ESP32-S3、8MB Flash），ADV 未验证。上传的是完整合并 .bin，烧录偏移 0x0，不是应用单文件。全量烧录会重置片内 WiFi 配置，SD 文件保留。

固件内仅有 29 词演示；完整英汉/汉英/拼音数据需从项目 sdcard/ 目录复制到 FAT32 microSD 根目录。已有词库无需重复拷贝，首次索引完成后从 SD 缓存快速启动。

- [项目与说明](https://github.com/RadioDisney/CardputerDict)
- [SD 词库目录](https://github.com/RadioDisney/CardputerDict/tree/master/sdcard)

应用代码 MIT；ECDICT MIT；CC-CEDICT 派生汉英和词组拼音数据 CC BY-SA 4.0。字体、dr_mp3 和其他依赖保留各自许可，见 third_party/。

源码提交：e77918149e850b87c9912632df98276839c7eece。封面由本版本实机界面截图拼合。