# CardputerDict 0.6.4 — 双语词典与完整中文维基百科

适用于普通M5Stack Cardputer与Cardputer-Adv，自动识别设备；同一通用镜像已经在两种硬件上测试。

- 英汉770,611词头、汉英198,650简繁词头、85,610拼音代码；繁体字回退、IPA、密排候选与英语朗读。
- 新增完整中文维基纯文字阅读：2026-11-01快照，1,563,854篇正文，4,863,298个标题/别名；磁盘检索、小缓冲、跨分片/超过4GB位置，文章不受原词典12KB上限截断。
- Tab/Shift+Tab按已安装SD词库循环；标准包为英汉→汉英→百科。Ctrl+Space切拼音/直接输入。
- 四方向键直接导航；Fn+方向键输入原标点，Fn+Shift输入上档标点。所有页面左上固定CARDPUTER DICT。
- 本版本数据从Git源码树迁到Releases，不再把源码ZIP当词库包。

## 必须准备SD数据

使用FAT32（不要exFAT）。完整百科+词典解压后约5.21GB，至少8GB卡、建议16GB或32GB，预留至少5.3GB空闲。下载/解压建议在电脑上完成。

1. 下载 `CardputerDict-0.6.4-SD-dictionaries.zip` 并解压（约24MB压缩）。
2. 需要百科时，下载 `CardputerDict-Wikipedia-zh-20261101-part-1-of-5.zip` 至 `part-5-of-5.zip` **全部5个独立ZIP**，全部解压到同一个目录。不要拼接ZIP，不要只下载一部分。合计约1.94GB下载。
3. 将 `dictionaries/`、`licenses/`、`audio/`、`wikipedia/` 直接复制到卡根目录。旧百科目录先整体改名备份，不要混装测试数据。
4. 安全推出，设备关机插卡再开机；首次原词库索引约1分钟，随后用缓存。维基索引已提前生成。

也可用源码里的自动下载与SHA-256校验：

```sh
python3 tools/download_release_data.py --release v0.6.4 --wikipedia --output ../Cardputer-SD
```

详见 [SD安装说明](https://github.com/RadioDisney/CardputerDict/blob/v0.6.4/docs/SD_INSTALL.md) 和 [百科说明](https://github.com/RadioDisney/CardputerDict/blob/v0.6.4/docs/WIKIPEDIA.md)。`data-assets.json`与`SHA256SUMS.txt`提供长度和校验摘要。

## 固件文件

- `CardputerDict-0.6.4-merged.bin`：ESP32-S3 / 8MB，全量镜像写入0x0，适用于M5Burner/新机，重置WiFi与音量设置；SD文件保留。
- `CardputerDict-0.6.4-app.bin`：现有同分区设备升级写入0x10000，保留片内设置。
- 合并镜像是构建产物，NVS区域为0xff，不含设备WiFi凭据；不附第三方发音录音包。

完整维基为无图纯文字整理，不等同网页版模板/Lua、图片、复杂公式排版；支持标题前缀检索而非正文全文搜索。生僻字受字体覆盖限制。

代码MIT，ECDICT MIT，CC-CEDICT派生词典及中文维基正文CC BY-SA4.0，其他数据/字体/依赖许可随包提供。每篇百科保留来源版本与历史/作者链接。
