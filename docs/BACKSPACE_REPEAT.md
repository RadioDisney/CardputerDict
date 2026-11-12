# 退格长按自动重复：0.4.1

2026-11-12（Asia/Shanghai）。

物理退格键按下立即触发一次，首次重复延迟 450ms，后续间隔 65ms，松手重置。Fn/Ctrl/Alt/Opt 组合及朗读期间不自动重复，必须松手后才重新启用；Fn+退格保持原有单次清空。

键盘矩阵仍逐键检测边沿，只有普通退格通过统一 KeyRepeat 定时器；其他按键行为不变。相同 handleKey 删除路径继续处理英文、拼音、完整中文 UTF-8 及 WiFi 输入字段。慢循环只产生一次重复，不补发积累删除。

KeyRepeat ASan/UBSan 测试通过：短按、450ms 延迟、65ms 周期、释放后停止、重新按下、修饰键抑制、慢帧不突发、millis 回绕。

已编译烧录，Flash 摘要校验通过。实机串口同一输入处理路径检查英文 abcdef→abcde、拼音 pingguo→pinggu、中文 你好→你、Fn+退格清空均通过。长按时序用主机测试验证，实体按键手感可直接试用。

记录见 docs/validation-v041/，测试为 tests/key_repeat_test.cpp，已接入 tools/test.sh。
