## CardputerDict 中英离线词典

适用于普通 M5Stack Cardputer（ESP32-S3、8 MB Flash），支持英汉 / 汉英双向查词、离线整词全拼和逐字输入，无需联网。

### 功能
- 英汉：770,611 个英文词头；汉英：198,650 个简繁体词头。
- 85,610 种整词拼音组合，支持 pingguo → 苹果、nihao → 你好；v 代表 ü。
- 中文和 IPA 音标显示，释义翻页；单行紧凑候选、数字选词。
- SD 索引自动缓存：首次生成后，实测软件重启约 1.32 秒进入界面（随 SD 卡而异）。

### 首次使用：必须准备 SD 词库
固件内仅带 29 词演示；完整词库不包含在 .bin 内。请下载项目源码 ZIP，将其中 sdcard/ 里面的 dictionaries/ 和 licenses/ 两个目录复制到 FAT32 microSD 卡根目录，关机后插卡再开机。首次会建立索引缓存，之后快速启动。

- [项目与完整说明](https://github.com/RadioDisney/CardputerDict)
- [SD 词库目录](https://github.com/RadioDisney/CardputerDict/tree/master/sdcard)
- 在 GitHub 点击 Code → Download ZIP，即可取得词库；也可使用项目 tools/upload_sd.py 通过 USB 安装。

### 按键
- Tab：英汉 / 汉英切换，同时清空输入。
- Enter：打开词条；拼音未选入时先选首项。
- 空格、1–9：选择当前页拼音候选；[ / ]：候选前后翻页。
- 释义页 Space 下一页；Fn+; 或 Fn+, 上一页；Fn+. 或 Fn+/ 下一页。
- Fn+; / Fn+.：列表上下选择，超出可见区域自动滚动。
- Backspace：删除字符；释义页返回。反引号键：返回 / 取消输入。
- Fn+Backspace：清空；Fn+H / G0：帮助；Fn+D：切换词库。
- Fn+R：重新加载 SD 缓存；Fn+Shift+R：强制完整校验并重建缓存。

### 烧录与兼容
这是从 0x0 烧录的完整合并固件，包含 bootloader、分区表、初始化数据和应用。已在普通 Cardputer 实机验证；Cardputer ADV 未验证，请勿选为该设备固件。烧录会替换现有应用；microSD 上的其他文件不会随烧录删除。

### 数据来源与许可
应用及原创演示代码：MIT。英汉数据来自 ECDICT（MIT）；汉英及词组拼音数据改编自 MDBG / CC-CEDICT，按 CC BY-SA 4.0 分发；逐字拼音来自 mozillazg/pinyin-data（MIT）。字体及依赖分别遵循各自许可。来源、修改说明和完整许可保留于项目 third_party/ 与 sdcard/licenses/。

当前不含语音、整句联想、自动词形还原；生僻字仍受字体覆盖限制。支持未压缩、UTF-8 纯文本 StarDict。