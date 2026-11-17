## CardputerDict 百科词典 — 0.6.4

适配 M5Stack Cardputer-Adv（ESP32-S3、8MB Flash）。同一固件自动识别普通 Cardputer / Cardputer-Adv，两种硬件均已实机测试。以下操作及数据说明对应0.6.4，新版数据通过GitHub Releases下载，源码ZIP不含完整词库。

### 功能
- 英汉770,611词头、汉英198,650简繁词头，85,610种整词拼音代码；中文/IPA音标、繁体补字、单行紧凑候选。
- 完整中文维基纯文字：2026-11-01快照，1,563,854篇正文、4,863,298个标题/简繁别名/有效重定向。按标题前缀查找，小内存逐页读取，不受原词典12KB上限截断；支持超过4GB数据位置与分片。
- Tab/Shift+Tab按SD已安装词库正/反向循环；标准包为英汉→汉英→百科。pinyin是输入辅助，不作为独立词库。
- Ctrl+Space在拼音/直接输入之间切换；pingguo→苹果，空格或1–9选词，Enter打开。v表示ü。
- 方向键直接导航、前后翻页，无需Fn；左右可翻拼音候选。[ / ]仍翻候选。Fn+方向键输入; . , /，加Shift输入: > < ?。
- Space/Enter阅读下一页，首尾循环；退格删除/返回，长按450ms后每125ms连续删除，Fn+Backspace清空。Fn+H/G0帮助，Fn+R重载。
- 左上角始终CARDPUTER DICT。封面按英文输入→释义→Tab汉英→拼音选苹果→释义→Tab百科→拼音选苹果→百科阅读的真实操作顺序播放，每帧一个UI。

### SD卡要求与下载（必须准备）
固件仅内置29词演示。使用FAT32，不能exFAT。仅英汉/汉英/拼音约62MB；完整百科加词库约5.21GB，至少8GB卡、建议16GB或32GB，并预留至少5.3GB空闲。

**[固件及全部SD数据下载：GitHub Releases v0.6.4](https://github.com/RadioDisney/CardputerDict/releases/tag/v0.6.4)**

1. 下载CardputerDict-0.6.4-SD-dictionaries.zip，解压得到dictionaries/、licenses/、audio/。
2. 需要百科时，再下载CardputerDict-Wikipedia-zh-20261101-part-1-of-5.zip至part-5-of-5.zip全部5个ZIP（合计约1.94GB）。它们是独立ZIP，全部解压到同一准备目录，不要拼接，也不能少任何一部分。
3. 把dictionaries/、licenses/、audio/、wikipedia/直接复制到SD根目录，不要再套一层准备目录。旧百科目录先整体改名备份，不要混装测试包。
4. 安全推出卡，将设备完全关机再插卡开机。首次原词库索引约1分钟，随后走缓存；维基索引已在电脑预先生成。

[图文/命令行安装说明](https://github.com/RadioDisney/CardputerDict/blob/v0.6.4/docs/SD_INSTALL.md) · [项目与操作说明](https://github.com/RadioDisney/CardputerDict/tree/v0.6.4)

Python用户可运行tools/download_release_data.py --release v0.6.4 --wikipedia --output ../Cardputer-SD，支持断点下载、SHA-256校验和解压。Release的data-assets.json与SHA256SUMS.txt提供文件清单和校验值。

### 英语朗读
Fn+P朗读/停止，Fn+= / Fn+-调音量；优先播放audio/en个人MP3/WAV，其次audio/cache/en。Fn+W在设备设置2.4GHz WiFi，缺少音频时通过验证HTTPS向dict.youdao.com发送所选英文词，获取并缓存，完成后关闭WiFi；第三方接口可用性可能变化。固件不附第三方录音包，不朗读中文百科全文。

### 烧录与限制
此处上传的是完整合并.bin，ESP32-S3/8MB，烧录地址0x0。全量烧录会重置片内WiFi与音量设置，SD数据保留。已有相同分区的用户可从GitHub下载应用文件写入0x10000保留设置。

百科为无图纯文字整理、正文转换简体；复杂模板/Lua、表格和公式不完全等同网页，个别格式参数可能残留，生僻字受字体覆盖限制。检索是标题前缀，不是正文全文搜索。

### 来源与许可
代码MIT，ECDICT MIT，CC-CEDICT派生汉英/词组拼音与中文维基正文CC BY-SA4.0。百科每篇保留来源版本及历史/作者链接；其他字体/依赖许可随项目与数据包保留。完整来源见Release的licenses/和wikipedia/LICENSE.txt。
