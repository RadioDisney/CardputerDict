# Release词库下载与SD安装

完整操作说明以[README的SD安装部分](../README.md#sd卡要求与安装)为准。数据从[Releases v0.6.4](https://github.com/RadioDisney/CardputerDict/releases/tag/v0.6.4)下载，不在源码ZIP里。

| 使用范围 | SD需求 | 下载内容 |
|---|---|---|
| 英汉/汉英/拼音 | FAT32，约62MB数据 | SD-dictionaries.zip |
| 加完整中文维基 | FAT32，至少8GB，建议16/32GB，至少5.3GB空闲 | 基础词库ZIP + 全部5个Wikipedia ZIP |

五个Wikipedia包是独立ZIP，全部解压到同一目录，不是需要拼接的多卷ZIP。每个包都小于2GiB。解压后的wikipedia/约5.15GB，正文单文件<=128MiB；它兼容FAT32而不需要exFAT。

新卡根目录应直接看到dictionaries/、licenses/、audio/、wikipedia/。已有百科目录先完整改名备份，不要把SAMPLE测试包与完整包混装。数据包无需也不应烧进片内Flash。

自动下载：

```sh
python3 tools/download_release_data.py --release v0.6.4 --wikipedia --output ../Cardputer-SD
```

校验解压包：

```sh
python3 tools/verify_wikipedia.py ../Cardputer-SD/wikipedia --require-complete
```

通过读卡器安装并校验（示例Mac卷名）：

```sh
python3 tools/install_wikipedia.py ../Cardputer-SD/wikipedia --sd-root /Volumes/CARD
```

不要使用原仓库sdcard/下的演示fixture覆盖Release的数据。标准包未包含demo独立词库，拼音辅助pinyin仍需保留。字典数据下载完成后安全推出，设备完全关机再插卡开机，等首次索引完成。

固定标题、方向键直接导航、Tab自动发现词库及Ctrl+Space输入切换均以0.6.4固件为准。完整合并固件写0x0会重置WiFi/音量，已有同分区设备可仅写应用到0x10000保留设置。
