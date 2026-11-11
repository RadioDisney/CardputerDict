# CardputerDict 0.2

普通 M5Stack Cardputer 上的离线英汉 / 汉英词典，支持整词全拼和逐字中文输入。使用 PlatformIO、Arduino、M5Cardputer、M5GFX；无需联网或 PSRAM。

屏幕保持 `setRotation(0)`，配合当前 M5GFX 的 Cardputer 面板偏移得到 **240×135 横屏**，画布读取实际屏幕尺寸。

## 使用

- **英汉**：输入 `apple`，按 Enter 查看中文释义。
- **汉英**：按 Tab，输入 `pingguo`，按空格或 1 选入“苹果”，再按 Enter 查看英文释义。
- **词组**：可以直接输入 `nihao` 选“你好”，不用逐字拼接。
- **逐字输入**：输入一个音节选字后，再输入下一个音节；已输入的中文会用于优先显示能组成词条的候选。
- **简繁体**：汉英词库包含简体和繁体词头；拼音默认输出简体。
- **ü**：使用 `v`，例如 `lv`、`lve`。

Tab 会同时切换词库和输入方式，并清空当前查询。第一次进入汉英模式会扫描汉英和拼音索引，等待进度条完成；此后英汉、汉英和拼音索引保留在内存，切换无需再次扫描。重启或 Fn+R 重载后会重新建立索引。

| 按键 | 操作 |
|---|---|
| Tab | 英汉英文输入 / 汉英拼音输入切换 |
| 字母 | 输入英文或全拼 |
| 空格、1–3 | 选择当前页候选词 |
| `[` / `]` | 拼音候选上一页 / 下一页 |
| Enter | 查询选中的词条；拼音尚未选入时先选首项 |
| Fn + `;` / Fn + `.` | 上下选择；释义页前后翻页 |
| Fn + `,` / Fn + `/` | 释义页前后翻页 |
| 空格 | 释义页下一页 |
| Backspace | 删除一个拼音字母或完整汉字；释义页返回 |
| 反引号键 | 返回列表；取消拼音或清空查询 |
| Fn + Backspace | 清空查询 |
| Fn + H / G0 | 帮助 |
| Fn + D | 切换其他 SD 词库或内置演示 |
| Fn + R | 重新挂载 SD，清空索引缓存并扫描词库 |

候选每页 3 项，支持整词提交。较长候选会在屏幕上省略，选入后保留完整词语。内置逐字候选表覆盖 413 个音节、6,763 个 GB2312 汉字；缺少 SD 拼音索引时仍可逐字输入。

## 数据

| 文件前缀 | 用途 | 索引词头数 |
|---|---|---:|
| ecdict | 英文 → 中文 | 770,611 |
| cedict | 中文 → 英文，简繁体与多音字合并 | 198,650 |
| pinyin | 无声调整词拼音 → 简体候选 | 85,610 |
| demo | 内置演示的 SD 版本 | 29 |

汉英数据来自 CC-CEDICT，共 125,403 条源记录；简繁体拆分为独立可查询词头，因此词头数与源记录数不同。词条可能包含一般词义、专名、分类词或用法说明，可翻页查看全部释义。

SD 根目录：

```text
dictionaries/
    ecdict.ifo   ecdict.idx   ecdict.dict
    cedict.ifo   cedict.idx   cedict.dict
    pinyin.ifo   pinyin.idx   pinyin.dict
    demo.ifo     demo.idx     demo.dict
licenses/
```

每个词库使用 UTF-8、未压缩、纯文本 `m`、32 位偏移的 StarDict。拼音索引作为内部数据使用，不会出现在 Fn+D 的词库切换列表中。

## 构建与烧录

```sh
cd /Users/huxunxun/WorkSpace/CardputerDict
pio run -e cardputer
pio device list
pio run -e cardputer -t upload --upload-port /dev/cu.usbmodemXXXX
```

使用数据 USB 线。无法识别串口时，按住 G0 插入 USB 后松开。项目使用 8 MB Flash、6 MB 应用分区；不配置 OTA。

合并固件 `CardputerDict-v0.2-factory.bin` 写入地址为 **0x0**。PlatformIO 构建目录的 `firmware.bin` 是单独应用，地址为 **0x10000**。设备已经烧录时无需再次操作。

原机完整 8 MB Flash 保存在 `backups/cardputer-before-20261110.bin`，恢复步骤见 `docs/RECOVERY.md`。

## 安装或添加 SD 词库

已经通过 USB 安装的数据无需重复拷贝。通过 USB 安装现有词库文件（电脑需 pyserial）：

```sh
python3 tools/upload_sd.py /dev/cu.usbmodemXXXX sdcard/dictionaries/ecdict
python3 tools/upload_sd.py /dev/cu.usbmodemXXXX sdcard/dictionaries/cedict
python3 tools/upload_sd.py /dev/cu.usbmodemXXXX sdcard/dictionaries/pinyin
```

脚本分块确认，校验传输内容，并回读 SD 校验后再启用新文件；相同文件跳过，同名但内容不同的文件拒绝覆盖。许可证和来源文件一并复制。传输中请保持供电；若断电遗留 `.文件名.part`，先用读卡器确认并移除该临时文件后重试。

也可关机取卡，用读卡器将项目 `sdcard/` 内的内容复制到已有 FAT32 卡的根目录，不要多套一层 sdcard 文件夹；不需要格式化或删除其他文件。`tools/copy_sd.py /Volumes/卡名` 会先显示计划，加 `--apply` 才实际复制。

若烧录或 USB 热复位后显示演示模式，先按 Fn+R。仍失败时，关闭电源并拔掉 USB 约 5 秒后重新开机。项目串口工具避免切换复位控制线。

## 生成数据

汉英和整词拼音索引：

```sh
python3 tools/prepare_cedict.py /path/to/cedict.txt.gz sdcard/dictionaries --stats third_party/cedict-stats.json
```

自建 TSV 或 ECDICT CSV：

```sh
python3 tools/prepare_dictionary.py mywords.tsv sdcard/dictionaries/mywords --name "我的词库"
python3 tools/prepare_dictionary.py /path/to/ecdict.csv sdcard/dictionaries/ecdict --format ecdict --name "ECDICT 英汉词典"
```

TSV 每行格式为 `词头<TAB>释义`，释义内用字面量 `\n` 表示换行。逐字拼音、演示和音标字形的生成脚本也保留在 tools/ 中；常规构建直接使用 include/ 下已生成的数据。

## 测试与诊断

```sh
sh tools/test.sh
python3 tools/device_check.py /dev/cu.usbmodemXXXX --expected-words 770611 --bilingual --output build/device
```

主机测试使用 AddressSanitizer / UndefinedBehaviorSanitizer，验证真实词库、索引边界、中文 UTF-8、词组选择、多音字、拼音翻页和转换工具。设备测试使用与实体键盘相同的按键处理函数，验证输入到查词的完整流程，并导出实际屏幕缓冲区。完整实体键盘手感可按上表试用。

串口诊断支持 `status`、`lookup apple`、`lookup 苹果`、`ime pingguo`、`type pingguo`、`press tab`、`press 1`、`press enter`、`screenshot`、`reload`。普通命令以 `CMD_DONE` 标记结束；二进制截图和文件传输有独立边界。

最新记录见 `docs/BILINGUAL_REPORT.md`，原始验证日志见 `docs/validation-v020/`。

## 范围与许可

支持整词全拼、逐字输入和短拼写前缀候选；不做整句自动分词、智能纠错或用户词频学习。查询使用词头定位，列表也可能显示排序相邻词。当前不读取 dict.dz、idx.gz、HTML、64 位偏移或 syn 别名索引，不支持发音和自动词形还原。单条释义最多读取 12 KB，超出时显示提示。

正文使用 efontCN_14 和 DejaVu Sans 生成的 Latin / IPA 后备字库；生僻字及字库范围外符号可能缺字。SD 拼音候选限制为 GB2312 字符。

- 应用代码及原创演示：MIT，见 LICENSE。
- ECDICT：MIT，来源和许可见 third_party/ECDICT-LICENSE.txt、third_party/sources.json。
- **CC-CEDICT 派生的 cedict.*、pinyin.* 数据：CC BY-SA 4.0**；来源、修改说明和许可见 third_party/CC-CEDICT-NOTICE.txt、CC-CEDICT-LICENSE.txt、cedict-source.json。原始数据由 MDBG 和 CC-CEDICT 社区维护。该数据许可与应用代码许可分别适用。
- 逐字拼音：mozillazg/pinyin-data，MIT。
- Latin / IPA 后备字形：DejaVu Sans 派生位图，许可和字体摘要见 third_party/DejaVuSans-LICENSE.txt、ipa-font-source.txt。
- M5Stack 和 IRremote 库保留各自许可，见 third_party/。
