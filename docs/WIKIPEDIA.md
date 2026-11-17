# 中文维基百科离线阅读 — CardputerDict 0.6.4

同一固件适配普通 Cardputer 和 Cardputer-Adv。按 **Tab** 统一循环切换英汉 → 汉英 → 百科三个模式，**Shift+Tab** 反向切换；百科数据从 SD 按需检索、按页读取，不要求 PSRAM。

## 使用

- Tab：切换下一词库；根据SD已安装词库生成列表，当前为英汉 → 汉英 → 百科 → 英汉。
- Shift+Tab：切换上一词库。
- Ctrl+Space：切换拼音 / 直接输入，保持当前词库；百科默认拼音，英文标题可切到直接输入。
- Fn+E、Fn+D 不再作为词库切换键。
- 拼音输入后按数字或空格选字/词，再按 Enter 打开百科文章；也可逐字组合专有名词。
- 检索按文章标题前缀进行，结果包含简繁别名及有效重定向；不提供正文全文搜索。
- ↑ / ↓：直接选择结果；Enter 打开。
- Space、Enter、↓ 或 →：下一页；↑ 或 ←：上一页。正文首尾循环。
- 拼音候选用 ← / → 或 [ / ] 翻页。
- Fn+方向键输入 ; . , /；Fn+Shift+方向键输入 : > < ?。
- Backspace 或反引号：从文章返回搜索；Fn+Backspace 清空。
- Fn+R：重新挂载数据。百科索引由电脑预先生成，不在设备开机时扫描百万词条。
- 原有英汉/汉英、拼音、英语朗读及中文补字保持可用。Fn+P 的英语朗读用于词典词条，不是中文百科全文朗读。

完整长文章不受词典原有12KB释义上限影响。翻页缓存固定为64个页位置，正文缓冲约1KB；首次从首页跳末页会按需定位，可按反引号取消。读到末页后已知总页数会缓存。

## 本次完整数据

- 来源：Wikimedia 官方 **2026-11-01** zhwiki pages-articles-multistream 快照。
- 原始压缩文件：3,635,267,339 字节；官方 SHA-1 `f7aaacab0f741bab3024e47eca1bbee8c8f625ee`，已核对一致。
- 1,563,854 篇主命名空间正文，4,863,298 个可检索标题（原题、简繁转换与有效重定向）。
- 正文4,964,649,705字节，索引97,265,960字节，标题87,218,278字节；整个百科目录约5.15GB（4.80GiB）。
- 正文分37片，每片不超过128MiB，兼容FAT32单文件限制；使用64位正文位置，超过4GB的词条同样可读。
- 474个无效、循环或目标不在主命名空间的重定向未加入检索。原始主条目正文保留。
- 此包为无图纯文字整理，转换为简体正文；表格转文本、链接标签保留。复杂模板/Lua生成内容不能完全展开，个别格式参数可能保留，纯模板/空页面显示无可读正文提示；生僻或扩展区字符仍受字体覆盖限制，不等同于网页版的排版与图片。
- 每篇保留来源版本URL和历史/作者链接，按 CC BY-SA 4.0 分发；完整说明见 wikipedia/LICENSE.txt、manifest.json。

小型测试包是94篇文章、174个标题，显示“测试/试读”，不能当作完整数据。完整包已在8GB FAT32卡的Adv设备实测，普通版使用同一通用固件并已通过测试包验证。

## 数据下载与SD安装

数据已迁移到[GitHub Releases v0.6.4](https://github.com/RadioDisney/CardputerDict/releases/tag/v0.6.4)，源码ZIP不包含完整数据。需要FAT32卡（不要exFAT），至少8GB、建议16GB或32GB，并预留至少5.3GB空闲。

基础英汉/汉英/拼音包约24MB；完整维基拆成5个独立ZIP（各约300–450MB，合计约1.94GB）。下载所有5部分并解压到同一目录，保留wikipedia/层次，不要拼接ZIP。连同基础词库解压后约5.21GB。

将dictionaries/、licenses/、audio/、wikipedia/直接放SD根目录。旧wikipedia/需先整体改名备份，不要混装测试版。详细步骤见[SD安装说明](SD_INSTALL.md)。

```sh
python3 tools/download_release_data.py --release v0.6.4 --wikipedia --output ../Cardputer-SD
python3 tools/install_wikipedia.py ../Cardputer-SD/wikipedia --sd-root /Volumes/CARD
```

完成后安全推出，将Cardputer关机再插卡开机。Tab按已安装词库循环；标准包为英汉、汉英、百科。完整包显示4,863,298个标题，且不带“测试/试读”。

## 以后更新数据

安装主机转换依赖（不装到Cardputer）：

```sh
python3 -m venv .venv-wiki
.venv-wiki/bin/python -m pip install -r tools/requirements-wiki.txt
```

使用官方XML(.bz2)快照转换，按对应官方校验值替换参数：

```sh
.venv-wiki/bin/python tools/prepare_wikipedia.py "/path/zhwiki-pages-articles.xml.bz2" "/path/new-wikipedia" --workers 4 --sha1 OFFICIAL_SHA1 --expected-bytes FILE_BYTES --source-url OFFICIAL_URL
python3 tools/verify_wikipedia.py "/path/new-wikipedia" --require-complete
```

构建过程中使用磁盘SQLite目录排序；内存只保存有限数量的待处理文章。`--resume` 可在相同源文件和校验值下恢复未完成转换；成功的数据目录不可作为继续构建目标。`--follow --expected-bytes ...` 可配合正在顺序追加的下载文件，下载无进展时按 `--follow-wait` 控制等待上限。

完整包建议通过读卡器复制。USB安装只允许不超过64MiB的测试包；`--replace-sample` 会先备份标明测试的小包，拒绝替换完整数据包。

## 固件与验证

0.6.1应用固件写入0x10000，可保留WiFi与音量。完整合并镜像写入0x0，用于新机初始化，会重置片内设置。两台当前设备已升级应用分区，设置保留。

已验证真实文章检索、简繁别名、拼音与英文输入、超过12KB的完整长文、跨分片读取、首尾循环、原词典和MP3朗读；完整数据在主机端已验证所有分片SHA-256、全部4,863,298条索引和实际超过4GB的正文读取。完整包已安装到8GB FAT32卡并完成Adv整卡实测，包括486万标题、超过4GB位置和跨分片长文。
