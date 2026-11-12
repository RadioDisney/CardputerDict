# CardputerDict 0.4.2

普通 M5Stack Cardputer 上的离线英汉 / 汉英词典，支持整词全拼和逐字中文输入。使用 PlatformIO、Arduino、M5Cardputer、M5GFX；无需联网或 PSRAM。

屏幕保持 `setRotation(0)`，配合当前 M5GFX 的 Cardputer 面板偏移得到 **240×135 横屏**，画布读取实际屏幕尺寸。

所有屏幕文字统一采用半角字符后 1 像素、全角/宽字符后 2 像素的额外字距；标题、输入框、词条列表、释义、候选、帮助、状态和进度提示共用同一渲染规则。换行、截断和候选分页同时计入这些间距。组合附加符号不额外拆开。

释义正文行间留白为 4 像素；当前字体高度 14 像素，行步进 18 像素，每页显示 4 行。绘制位置、翻页边界和页码共用同一行数计算。

备选词条列表的行步进由 15 增加到 17 像素。普通状态同屏显示 4 条，拼音输入时显示 2 条；上下选择自动滚动，全部匹配词条仍可选。

释义翻页支持首尾循环：末页继续下一页回首页，首页继续上一页跳末页。适用于 Space、Enter、Fn 翻页组合；单页词条始终停留在该页。

## 英语朗读（0.4.0）

在英文词条列表或释义页按 **Fn+P** 朗读当前词条；再次 Fn+P 或反引号键停止。**Fn+= / Fn+-** 调整音量。默认音量约 38%，设置会保留。

发音按以下顺序查找：

1. `/audio/en/<词名>.mp3` 或 `.wav`（用户导入的离线音频）。
2. `/audio/cache/en/<词名>.mp3`（已联网获取的缓存）。
3. 没有本地音频时，连接已配置的 WiFi，经 HTTPS 请求有道英文单词发音，写入 SD 并回读校验，之后播放。

**Fn+W** 打开 WiFi 设置：输入 2.4 GHz SSID，Enter 到密码栏，输入密码后 Enter 保存；Tab 切换字段，反引号取消。密码显示为星号，保存在本机 NVS，串口和发布固件不包含用户凭据。此版本用实体键盘输入 ASCII SSID/密码。开放网络可留空密码；普通 WPA/WPA2 密码为 8–63 字符。

不会开机自动连接 WiFi。仅在主动朗读且缺少本地音频时联网，获取完成或失败后关闭 WiFi；在线请求会把所选英文词发送到 `dict.youdao.com`，不会发送 WiFi 密码。使用根证书和 NTP 校时验证 HTTPS，不跳过证书校验。该服务的可用性和返回语音受服务端影响；离线文件与已有缓存不受网络影响。

支持 MP3，以及 8/16-bit PCM WAV，单声道或双声道，8–48 kHz。每个文件上限 512 KB、最长 30 秒。使用 16 KB 压缩输入缓冲和三块 PCM 缓冲流式播放，不把整个音频解码到 RAM。当前只朗读 ASCII 英语词/短语，支持字母、数字、空格、撇号、连字符和句点；不直接朗读中文词头，不做全离线任意单词 TTS。

输入受支持字符时将词头转为小写，文件名中空格、撇号和句点等按百分号编码，例如 `apple butter` 对应 `apple%20butter.mp3`。长词可用下面工具生成正确文件名。

### 离线音频导入

准备按英文词头命名的 MP3/WAV 文件，例如 Apple.mp3、hello.wav。批量生成可复制的 SD 目录（标准 Python 即可）：

```sh
python3 tools/prepare_audio_pack.py /path/to/your-pronunciations /path/to/staging
```

把 staging 内生成的 `audio/` 目录复制到 FAT32 SD 卡根目录。脚本会拒绝覆盖不同内容的已有文件。可导入自己的常用词音频集；项目没有附带数万个单词的录音包，网络缓存按实际使用逐词积累。音频来源的许可分别适用。

也可通过 USB 传到机内 SD（电脑需要 pyserial）：

```sh
python3 tools/install_audio.py /dev/cu.usbmodemXXXX apple /path/to/apple.mp3
python3 tools/install_audio.py /dev/cu.usbmodemXXXX hello /path/to/hello.wav
```

目录示例：

```text
audio/
    en/apple.mp3
    en/hello.wav
    en/apple%20butter.mp3
    cache/en/radio.mp3
```

播放期间可用 Fn+P / 反引号停止。WiFi 连接和获取有超时；进入 HTTPS 建连/请求阶段后，按键可能需等待本次有限时长请求返回。下载流传输期间会检查取消按键，不会启用不完整的 `.part` 缓存。SD 损坏、文件格式不支持、没有网络或证书验证失败均会显示提示。

联网和低内存朗读会临时释放词典索引，播放/停止后从缓存恢复。索引使用最多 4 KB 的分块存储与流式恢复，适应 WiFi 使用后的堆碎片；原来的 cidx 文件格式兼容。扬声器在结束后释放 I2S 和任务缓冲。

实机已安装 apple 的 MP3 和 hello 的 WAV 供试用，并验证了 language、screen 等词首次联网获取及离线重播；这类服务音频仅存设备 SD，源码和发布包不附带这些录音。

### 朗读诊断与测试

```sh
python3 tools/check_pronunciation.py /dev/cu.usbmodemXXXX --output build/pronunciation
# 需要预先在设备配置 WiFi；该词应尚未缓存：
python3 tools/check_pronunciation.py /dev/cu.usbmodemXXXX --online-word pronunciation --output build/pronunciation
```

串口支持 `speak apple`、`speaklocal apple`（禁止联网）、`audio status`、`audio stop`、`wifi`，以及 `press pronounce`（共用 Fn+P 处理逻辑）。设置页也可从设备 Fn+W 打开，不必将密码发送到电脑。构建时使用的 dr_mp3 解码器及根证书来源见 third_party/ 与 include/pronunciation_ca.h。

通过 PlatformIO 更新应用会保留现有 WiFi 设置；从 0x0 全量刷入合并固件用于全新安装时，片内设置会重置，需要重新配置 WiFi，SD 音频仍保留。

验证记录见 `docs/PRONUNCIATION.md` 和 `docs/validation-v040/`。

退格支持按住连续删除：首次按下立即删除一次，约 450ms 后开始每 125ms 重复，松手停止。适用于查询、拼音和 WiFi 输入框；中文按完整 UTF-8 字符删除。Fn+退格保持单次清空，其他修饰键组合不自动重复。

键盘采用独立 1ms 扫描任务，按键变化经 2ms 去抖后入队；界面刷新和查词期间也持续捕获松手/重按，同键快速连按无需等待界面刷新结束。长按退格时延后匹配词条刷新，松手后自动更新。

## 使用

- **英汉**：输入 `apple`，按 Enter 查看中文释义。
- **汉英**：按 Tab，输入 `pingguo`，按空格或 1 选入“苹果”，再按 Enter 查看英文释义。
- **词组**：可以直接输入 `nihao` 选“你好”，不用逐字拼接。
- **逐字输入**：输入一个音节选字后，再输入下一个音节；已输入的中文会用于优先显示能组成词条的候选。
- **简繁体**：汉英词库包含简体和繁体词头；拼音默认输出简体。
- **ü**：使用 `v`，例如 `lv`、`lve`。

Tab 会同时切换词库和输入方式，并清空当前查询。每套词库首次使用时扫描并校验索引，随后序列化保存到同名 `.cidx` 文件。后续开机、Tab 首次切换或 Fn+R 重载时直接读取缓存；同次开机中仍保留内存索引。缓存缺失、校验失败或词库检查不匹配时自动重新扫描。

| 按键 | 操作 |
|---|---|
| Tab | 英汉英文输入 / 汉英拼音输入切换 |
| 字母 | 输入英文或全拼 |
| 空格、1–9 | 选择当前页候选词 |
| `[` / `]` | 拼音候选上一页 / 下一页 |
| Enter | 查询选中的词条；拼音尚未选入时先选首项 |
| Fn + `;` / Fn + `.` | 上下选择；释义页前后翻页 |
| Fn + `,` / Fn + `/` | 释义页前后翻页 |
| 空格 | 释义页下一页 |
| Backspace | 删除一个拼音字母或完整汉字；释义页返回 |
| 反引号键 | 返回列表；取消拼音或清空查询 |
| Fn + Backspace | 清空查询 |
| Fn + H / G0 | 帮助 |
| Fn + P | 朗读当前英文词条 / 停止 |
| Fn + W | 设置 WiFi |
| Fn + = / Fn + - | 增加 / 减少音量 |
| Fn + D | 切换其他 SD 词库或内置演示 |
| Fn + R | 重新挂载 SD，从磁盘索引缓存快速加载 |
| Fn + Shift + R | 完整重新校验启动词库、汉英与拼音，并重建 SD 索引缓存 |

候选保持单行和原字号，按实际字宽紧密排列，每页数量随词长变化；数字编号与当前页一一对应。240 像素屏幕上，通常可显示 8 个单字候选或 5 个双字词。支持整词提交。较长候选会在屏幕上省略，选入后保留完整词语。内置逐字候选表覆盖 413 个音节、6,763 个 GB2312 汉字；缺少 SD 拼音索引时仍可逐字输入。

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

合并固件 `CardputerDict-v0.4.2-factory.bin` 写入地址为 **0x0**。PlatformIO 构建目录的 `firmware.bin` 是单独应用，地址为 **0x10000**。设备已经烧录时无需再次操作。

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
python3 tools/device_check.py /dev/cu.usbmodemXXXX --expected-words 770611 --bilingual --compact --output build/device
```

主机测试使用 AddressSanitizer / UndefinedBehaviorSanitizer，验证真实词库、索引边界、中文 UTF-8、词组选择、多音字、拼音翻页和转换工具。设备测试使用与实体键盘相同的按键处理函数，验证输入到查词的完整流程，并导出实际屏幕缓冲区。完整实体键盘手感可按上表试用。

串口诊断支持 `status`、`reindex`、`restart`、`lookup apple`、`lookup 苹果`、`ime pingguo`、`type pingguo`、`press tab`、`press 1`、`press enter`、`screenshot`、`reload`。普通命令以 `CMD_DONE` 标记结束；二进制截图和文件传输有独立边界。

备选词条列表行距更新见 `docs/WORD_LIST_SPACING.md` 和 `docs/validation-v027/`。行距与 TESLA 音标修复见 `docs/TESLA_AND_LINE_GAP.md` 和 `docs/validation-v026/`。历史释义行距更新见 `docs/DEFINITION_LINE_GAP.md` 和 `docs/validation-v025/`。全局字距更新见 `docs/GLOBAL_TYPOGRAPHY.md` 和 `docs/validation-v024/`。历史释义字距更新见 `docs/DEFINITION_SPACING.md` 和 `docs/validation-v023/`。单行紧凑候选更新见 `docs/COMPACT_CANDIDATES.md` 和 `docs/validation-v021/`；双向词典验证另见 `docs/BILINGUAL_REPORT.md`。

## 范围与许可

支持整词全拼、逐字输入和短拼写前缀候选；不做整句自动分词、智能纠错或用户词频学习。查询使用词头定位，列表也可能显示排序相邻词。当前不读取 dict.dz、idx.gz、HTML、64 位偏移或 syn 别名索引，不支持自动词形还原。单条释义最多读取 12 KB，超出时显示提示。

正文使用 efontCN_14 和 DejaVu Sans 生成的 Latin / IPA / Cyrillic 后备字库（包括 TESLA 音标中的 ә）；生僻字及字库范围外符号可能缺字。SD 拼音候选限制为 GB2312 字符。

- 应用代码及原创演示：MIT，见 LICENSE。
- ECDICT：MIT，来源和许可见 third_party/ECDICT-LICENSE.txt、third_party/sources.json。
- **CC-CEDICT 派生的 cedict.*、pinyin.* 数据：CC BY-SA 4.0**；来源、修改说明和许可见 third_party/CC-CEDICT-NOTICE.txt、CC-CEDICT-LICENSE.txt、cedict-source.json。原始数据由 MDBG 和 CC-CEDICT 社区维护。该数据许可与应用代码许可分别适用。
- 逐字拼音：mozillazg/pinyin-data，MIT。
- Latin / IPA 后备字形：DejaVu Sans 派生位图，许可和字体摘要见 third_party/DejaVuSans-LICENSE.txt、ipa-font-source.txt。
- M5Stack 和 IRremote 库保留各自许可，见 third_party/。

Unicode 字符宽度分类数据的来源和许可见 `third_party/Unicode-NOTICE.txt`、`Unicode-LICENSE.txt`；可用 `python3 tools/prepare_spacing_data.py` 重新生成。

## SD 索引缓存与快速启动（0.3.0）

缓存保存的是原来内存中的稀疏锚点（每 64 词条一个 32 位偏移），不改变 StarDict 源文件。自动生成在同目录：`ecdict.cidx`、`cedict.cidx`、`pinyin.cidx`。它们是可重建的派生文件，无需手动拷贝；删除后只会触发下一次完整扫描。

缓存由固定字节序的 64 字节头和锚点数组组成，包含格式版本、步长、词条数、源文件长度、修改时间、ifo 全文 CRC、索引/正文抽样 CRC，以及缓存头和锚点各自的 CRC32。加载时校验结构、单调偏移和少量锚点边界。

首次扫描仍需等待，但只做一次；文件正常时，后续无需遍历数十 MB 的源索引。缓存先写临时文件并回读核对，再发布为 `.cidx`；中途断电、缓存文件截断或不可写都能回退为源索引扫描，缓存写入失败也不影响当前查词。

源文件检查使用大小、修改时间和固定位置内容抽样，避免开机时重新计算整个大文件的哈希。**如果在外部原地修改词库，同时保留文件长度和修改时间，且改动未落在抽样区域内，不能保证自动发现。** 这种操作后按 Fn+Shift+R，或删除对应 cidx，完整重建一次；项目 USB 安装新词库时会主动清除对应缓存。

串口 `reload` 使用缓存，`reindex` 强制重新校验启动词库/汉英/拼音，`restart` 重启。`status` 会显示各词库 cache/scan 加载方式、耗时、是否写入缓存及启动完成时间。

缓存与重启基准测试：

```sh
python3 tools/benchmark_startup.py /dev/cu.usbmodemXXXX --output build/startup
# 包含完整重新校验及三个软件重启：
python3 tools/benchmark_startup.py /dev/cu.usbmodemXXXX --rebuild --output build/startup
```

验证记录见 `docs/INDEX_CACHE.md` 与 `docs/validation-v030/`。

MP3 解码器采用 dr_mp3（David Reid，MIT-0；基于 minimp3），以 vendored 快照固定源文件，见 `third_party/dr_mp3-LICENSE.txt` 与 `dr_mp3-source.json`。
