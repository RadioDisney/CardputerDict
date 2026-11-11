# CardputerDict 0.2 双向词典与拼音输入验证

日期：2026-11-10（Asia/Shanghai）。项目：`/Users/huxunxun/WorkSpace/CardputerDict`。

## 结果

已编译、烧录到连接的普通 Cardputer，并将汉英和整词拼音数据安装到机内 SD。SD 新文件经过传输校验和回读校验，原英汉数据保留。

- 英汉：770,611 个英文词头。
- 汉英：198,650 个简繁体词头，源数据 CC-CEDICT 共 125,403 条记录，多音和多义记录合并。
- 整词拼音：85,610 种无声调拼写组合；内置逐字候选仍可用。
- `setRotation(0)`，实机返回 `display=240x135 rotation=0`。

Tab 同时切换词库和输入方式、清空当前查询；首次加载后保留英汉、汉英和拼音索引。此次缓存切换测量为 0.036 秒，包含串口往返，实际耗时随设备状态变化。全部三个索引加载后可用堆约 108 KB。SD 文件句柄上限设为 16，支持同时打开三套索引和正文。

## 实机验证

- `apple` 返回中文释义，缺失单词返回 NOT_FOUND。
- `苹果`、`蘋果` 均返回英文释义。
- 通过与实体键盘共用的处理函数执行 Tab、键入 pingguo、1 选词、Enter 查词，完整进入“苹果”的英文释义。
- `nihao` 空格选入“你好”；退格后为完整汉字“你”。
- 候选 `[` / `]` 翻页、`v` 代表 ü 验证通过。
- 屏幕缓冲区已导出并目视检查：整词候选每页 3 项，中文和音标排版正常。实体键盘的完整手感仍可按 README 试用。

最终汉英状态：

```text
CardputerDict 0.2.0 board=14 heap=108192 sd=1 words=198650 source=CC-CEDICT 汉英 direction=zh-en
display=240x135 rotation=0
lexicons en=770611 zh=198650 ime=85610
note=
CMD_DONE
```

## 构建与测试

PlatformIO espressif32 6.12.0，M5Cardputer 1.1.1，M5Unified 0.3.0，M5GFX 0.3.0。最终静态 RAM 28,440 字节，程序 Flash 1,433,057 字节。完整编译和写入摘要校验通过。

主机测试采用 AddressSanitizer / UndefinedBehaviorSanitizer，覆盖 10,000 次索引对照查询、重复词头边界、损坏词库、UTF-8 删除/截断、整词提交、候选分页、长度限制；并验证真实英汉、简繁汉英和拼音词库。Python 转换工具 6 项测试通过。Git diff 空白检查通过。

日志及原生截图：`docs/validation-v020/`。

## 文件与许可

CC-CEDICT 官网来源、压缩文件 SHA-256 和修改说明见 `third_party/cedict-source.json`、`CC-CEDICT-NOTICE.txt`。派生的 cedict.* 和 pinyin.* 数据按 CC BY-SA 4.0 分发，应用代码仍使用 MIT。许可证已经复制到 SD。

合并固件写入地址 0x0；设备已经更新，无需重复烧录或拷卡。SD addon 包只含新增汉英和拼音数据，需要已有 ecdict 词库。完整数据目录仍保留在项目 sdcard/。

## 范围

支持整词全拼、逐字输入与拼写前缀候选；不含整句输入、自动纠错或词频学习。拼音候选限 GB2312，生僻字仍受字体覆盖限制。首次加载索引需要等待进度条，重启或 Fn+R 后会重新扫描。

原固件备份和恢复说明继续保留在 backups/ 与 docs/RECOVERY.md。
