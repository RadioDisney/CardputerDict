# 英语单词朗读：CardputerDict 0.4.0

日期：2026-11-12（Asia/Shanghai）。项目：`/Users/huxunxun/WorkSpace/CardputerDict`。

已实现并烧录到当前普通 Cardputer。用户已在设备配置 WiFi，并确认听到 apple / hello 朗读。

## 使用

- Fn+P：朗读当前选中的英文词条；再按一次停止。
- Fn+W：设备端输入 2.4 GHz SSID/密码，Tab 切字段，Enter 下一步/保存；反引号取消。
- Fn+= / Fn+-：音量调节，保存到本机配置。
- 朗读顺序：SD 用户音频 audio/en → SD 联网缓存 audio/cache/en → 已配置 WiFi 的 HTTPS 获取并缓存。
- 不开机自动联网，获取成功或失败后关闭 WiFi。已缓存文件可断网播放。

## 实现与内存

使用 dr_mp3 流式 MP3 解码，输入块 16 KB，PCM 三缓冲；支持 PCM WAV 8/16-bit、mono/stereo、8–48 kHz。每文件最多 512 KB、最长 30 秒。当前英文词头及短语按小写和百分号规则定位文件。不是离线任意单词 TTS；无录音时需要网络，或自行导入发音文件。

默认网络来源为 https://dict.youdao.com/dictvoice?audio=WORD&type=2。使用 DigiCert Global Root G2 验证证书，并在需要时 NTP 校时，不调用 setInsecure。仅发送所选英文词，不发送 WiFi 密码。第三方接口可用性可能变化。

下载写临时 .part 文件，限制长度和超时，关闭 TLS 后回读 CRC 校验、验证 MP3，再重命名为正式缓存。用户导入文件不被覆盖。

网络获取和低内存播放会暂时释放字典索引，结束/停止后从 SD 缓存恢复。为适应 WiFi 后的堆碎片，索引改为每块最多 4 KB 的存储，缓存加载为流式读入，不再同时申请两份大数组；分配失败返回错误，不抛出大块分配异常。cidx 格式与 0.3.x 兼容。扬声器结束时释放 I2S 和任务缓冲。

## 实机验证

- apple.mp3：48 kHz mono，56,208 PCM 帧，峰值 24,918，与主机解码结果一致。
- hello.wav：48 kHz mono 16-bit，60,096 帧，峰值 25,265。
- 短小 WAV 8-bit stereo 样本：16 kHz，2,400 帧，峰值 5,120，验证位深转换和立体声播放。
- language 首次联网获取并缓存：66,288 帧、峰值 25,259；随后 WiFi=off 的 speaklocal 重播成功。
- screen 网络获取及缓存重播通过，返回音频为 44.1 kHz stereo，86,400 帧、峰值 23,168。
- 加载英汉/汉英/拼音全部索引后反复播放 MP3/WAV，查询“苹果”和 pingguo 仍正常；停止、非法中文朗读请求和连接阶段取消均处理正确。
- 通过与实体 Fn+P 同一处理函数验证朗读和停止。
- 真实软件重启后 WiFi 配置保留、WiFi 未连接，language 缓存继续离线播放。启动约 1.42 秒，英汉索引缓存加载约 0.39 秒。
- 最终设备仍为 sd=1、770,611 英文词头、240×135、rotation=0。

## 主机与构建

MP3 原生测试比较内存解码和 16 KB 回调流解码，帧数和峰值一致。WAV/命名测试覆盖 mono/stereo、8/16-bit、填充块、损坏和截断数据、URL/文件名边界。原有 cidx 损坏、源文件变化、字距和候选分页测试均通过 ASan/UBSan。已有 Python 9 项及新增音频工具 2 项测试通过。

最终静态 RAM：66,028 字节；Flash：2,071,625 字节。编译、USB 写入及 Flash 摘要校验通过。日志见 docs/validation-v040/。

## 音频导入与发布范围

`tools/prepare_audio_pack.py` 可把按词头命名的个人 MP3/WAV 集合生成可复制的 audio/en 目录；`tools/install_audio.py` 通过 USB 安装并回读校验；`tools/check_pronunciation.py` 用于实机回归。

当前 SD 已有 apple/hello 的本地试听文件和测试中实际获取的词语缓存。源码和公开交付包不包含这些第三方服务录音，也不包含用户 WiFi 凭据；音频许可按各来源分别处理。

原有屏幕、间距、循环翻页、右上角 Tab 提示等继续保留。本次为本地固件更新，M5Burner 的 0.3.0 上传记录保留。
