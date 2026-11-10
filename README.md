# CardputerDict

普通 M5Stack Cardputer（ESP32-S3 / 8 MB Flash）上的离线英汉词典，使用 PlatformIO、Arduino、M5Cardputer 和 M5GFX。不依赖网络，不需要 PSRAM。

屏幕初始化使用 `setRotation(0)`；当前 M5GFX 已包含 Cardputer 的面板旋转偏移，对应 240×135 横屏。画布从屏幕读取实际尺寸。

## 已实现

- 英文键盘输入、忽略 ASCII 大小写的词条定位、显示匹配词及后续相邻词。
- 中文释义、UTF-8 中英混排、释义分页；中文字体为 efontCN 12 / 14 / 16，正文搭配 DejaVu Sans 生成的 Latin / IPA 位图后备字体。
- 离线逐字全拼：413 个拼音、6,763 个 GB2312 汉字；支持多音字候选，`v` 代替 `ü`。
- 从 microSD 读取未压缩、纯文本、32 位偏移的 StarDict；最多扫描 16 个词库。
- 启动时顺序校验索引，每 64 个词条保留一个位置。770,611 个词条的稀疏索引约占 48 KB RAM，正文按需读取。
- 无 SD 卡或词库不可用时，自动使用 29 词的内置演示，包含少量汉英词条。
- 串口诊断：`status`、`lookup apple`、`reload`。
- USB 串口导入词库：分块确认、传输校验、SD 回读校验后重命名，保留已有不同内容的文件。

## 编译与烧录

```sh
cd /Users/huxunxun/WorkSpace/CardputerDict
pio run -e cardputer
pio device list
pio run -e cardputer -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor --port /dev/cu.usbmodemXXXX --baud 115200
```

使用数据 USB 线。如果看不到串口，按住正面的 G0 键，插入 USB，再松开。烧录后若未自动运行，按复位键；必要时将电源开关打开。串口名称可能在复位后改变。

实机检查中遇到过 USB 热复位后 SD 卡暂时不响应的情况。固件已加入重试和降速回退；若仍显示“演示 29词”，先按 Fn+R。仍失败时，将开关拨到 OFF、拔掉 USB，等待约 5 秒后重新开机。日常使用尽量使用本项目的串口脚本，它们会避免切换复位控制线。

本项目使用 6 MB 应用分区，不配置 OTA。首次烧录会写入项目分区表。若需恢复原固件，请使用烧录前保存的完整 Flash 备份。

## 通过 USB 安装词库（推荐）

Cardputer 内插好 FAT32 microSD 卡并运行本固件后，可以直接通过数据 USB 线安装，无需取卡。电脑需安装 `pyserial`。

```sh
python3 tools/upload_sd.py /dev/cu.usbmodemXXXX sdcard/dictionaries/demo
python3 tools/upload_sd.py /dev/cu.usbmodemXXXX sdcard/dictionaries/ecdict
```

脚本会同时复制 `sdcard/licenses` 下的许可证及来源文件到 SD 的 `dictionaries/`。完全相同的文件校验后跳过，同名不同内容的文件拒绝覆盖。传输期间请保持 USB 连接和设备供电；完成后自动重载词库。如果意外断电遗留 `.文件名.part`，请通过读卡器确认并移除对应临时文件后重试。

## 使用读卡器准备 SD 卡

1. 关闭 Cardputer 电源，取出 microSD，插入电脑读卡器。
2. 使用现有 FAT32 卡即可；不必为本项目格式化或清空其他文件。不支持 exFAT。若卡是 exFAT，先换一张 FAT32 卡或自行备份后再处理。
3. 把项目 `sdcard/` **里面的内容**复制到 SD 根目录。不要额外套一层 `sdcard` 文件夹。
4. 安全推出卡，放回 Cardputer，开机。应用优先选择 `ecdict.ifo`。首次启动扫描 17 MB 索引，请等待进度条完成。

期望目录：

```text
SD根目录/
├── README.txt
├── licenses/
└── dictionaries/
    ├── ecdict.ifo
    ├── ecdict.idx
    ├── ecdict.dict
    ├── demo.ifo
    ├── demo.idx
    └── demo.dict
```

也可以运行 `python3 tools/copy_sd.py /Volumes/你的SD卡名称`，先查看复制计划，再加 `--apply` 执行。脚本不会格式化磁盘；遇到同名不同内容文件时默认拒绝覆盖。

固件不会把 microSD 映射成电脑上的 U 盘；USB 安装需使用上面的专用脚本，或改用读卡器。

## 按键

| 按键 | 操作 |
|---|---|
| 字母 / 数字 | 输入查询文本；中文模式下字母输入拼音 |
| Enter | 打开选中词条；拼音未完成时先选第一个候选 |
| Tab | 英文 / 中文输入切换 |
| Fn + `;` / Fn + `.` | 上 / 下选择；释义页中前 / 后翻页 |
| Fn + `,` / Fn + `/` | 前 / 后翻页 |
| 空格 | 拼音首选；释义页下一页 |
| 1–5 | 拼音候选选字 |
| `[` / `]` | 拼音候选前 / 后翻页 |
| Backspace | 删除拼音或一个完整 UTF-8 字符；释义页返回 |
| 反引号键 | 返回列表；列表中取消拼音或清空查询 |
| Fn + Backspace | 清空查询 |
| Fn + H / 正面 G0 | 显示 / 关闭帮助 |
| Fn + D | 切换下一 SD 词库；末尾回到内置演示 |
| Fn + R | 重新挂载 SD 并扫描词库 |

先试 `apple`、`dictionary`、`run`。输入中文后，必须选择含中文词头的词库才有对应结果：**ECDICT 是英文词头的英汉词库，不提供中文反向检索**。切到 demo 可以尝试拼音 `ping` 选“苹”、`guo` 选“果”，查询“苹果”。

## 导入自己的词库

支持 `.ifo + .idx + .dict`，要求：UTF-8、`sametypesequence=m`、32 位偏移，词头最多 255 字节，词条总数不超过 1,000,000。StarDict 的排序规则为 ASCII 忽略大小写比较，再按原始 UTF-8 字节比较。

当前不读取 `.dict.dz`、压缩 `.idx.gz`、HTML 词条、64 位偏移或 `.syn` 别名索引。请先在电脑端解压或转换。不支持自动词形还原、模糊搜索、发音和词组拼音联想。查询不存在的词时会显示按排序靠近的词，列表并不保证每项都是前缀匹配。

可以用 TSV 自建词库，每行 `词头<TAB>释义`，释义内用字面量 `\n` 表示换行：

```sh
python3 tools/prepare_dictionary.py mywords.tsv sdcard/dictionaries/mywords --name "我的词库"
```

重新生成演示和拼音数据：

```sh
python3 tools/prepare_dictionary.py data/demo.tsv sdcard/dictionaries/demo --name "Cardputer Demo 英汉与汉英" --demo-header include/demo_data.h
python3 tools/prepare_pinyin.py /path/to/pinyin.txt include/pinyin_data.h
```

原始 ECDICT CSV 的转换：

```sh
python3 tools/prepare_dictionary.py /path/to/ecdict.csv sdcard/dictionaries/ecdict --format ecdict --name "ECDICT 英汉词典"
```

打包提供的 `CardputerDict-factory.bin` 是包含 bootloader、分区表和应用的合并固件，写入地址为 `0x0`；PlatformIO 构建目录中的 `firmware.bin` 只是应用，写入地址为 `0x10000`，两者不要混淆。

释义单次最多读取 12 KB，超出时显示截断提示。内置中文字体覆盖有限，生僻字和后备字库范围外的符号可能缺字；拼音候选限制为 GB2312 汉字。小字号英文会按屏幕宽度换行，可能拆开单词。

## 验证

```sh
sh tools/test.sh
```

主机测试使用 AddressSanitizer / UndefinedBehaviorSanitizer，验证索引查找、跨稀疏索引边界的重复词头、损坏数据、UTF-8 删除与截断，并对实际 SD 词库校验。硬件测试记录见 `docs/BUILD_REPORT.md`。

运行硬件验证并抓取实际屏幕缓冲区（电脑需 `pyserial` 和 `Pillow`）：

```sh
python3 tools/device_check.py /dev/cu.usbmodemXXXX --output build/device
```

## 来源与许可

- 项目应用和自编演示词条：MIT，见 `LICENSE`。
- ECDICT：<https://github.com/skywind3000/ECDICT>，MIT；使用当前下载文件所附许可证，见 `third_party/ECDICT-LICENSE.txt`。
- 汉字拼音：<https://github.com/mozillazg/pinyin-data>，MIT，见 `third_party/pinyin-data-LICENSE.txt`。
- 正文 Latin / IPA 后备字形：从 DejaVu Sans 生成 CardputerIpa13 位图子集，原字体许可及来源摘要见 `third_party/DejaVuSans-LICENSE.txt` 和 `third_party/ipa-font-source.txt`。
- 下载来源、日期、大小与 SHA-256：`third_party/sources.json`。
- M5Stack 库及其内嵌字体保留各自许可证，随 PlatformIO 依赖安装。
