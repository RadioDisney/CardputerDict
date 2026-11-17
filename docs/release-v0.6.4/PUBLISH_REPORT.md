# v0.6.4 GitHub 与 M5Burner 发布记录

## GitHub

源码提交：`4b69cef2c59ef443eea191aa8e1b6bbef6178595`。已推送feature/Wiki，并确认默认分支master是旧提交祖先后快进更新。标签v0.6.4指向同一提交，未强制推送或重写历史。

Release公开地址：https://github.com/RadioDisney/CardputerDict/releases/tag/v0.6.4

12个公开附件：应用与合并固件、基础词库ZIP、5份中文维基ZIP、data-assets.json、SHA256SUMS.txt、SD-INSTALL.txt及顺序操作GIF。全部GitHub服务器端SHA-256、文件长度和uploaded状态与本地一致，匿名下载data-assets.json也匹配。

大数据从当前Git树移除，本地原数据保留并忽略；历史提交未重写。源码仍保留小型demo测试fixture与数据许可证。完整数据仅从Release获取。新增自动下载/断点续传/校验/安全解压工具，六个ZIP整套安装演练及完整维基验证通过。主机11个原生测试及23个Python用例通过。

## M5Burner

两种设备的0.6.4均在现有条目上传，版本列表明确确认“待审核 / 公开”：

- Cardputer：https://burner.m5stack.com/developer/firmware/2120468585455984642/versions
- Cardputer ADV：https://burner.m5stack.com/developer/firmware/2122146817619476482/versions

两个条目的公共资料名称均提交为 **CardputerDict 百科词典**。网站公共资料采用单独审核，已保存且显示“资料审核中”；审核通过前页面仍显示旧名称和旧公共描述，不应视为改名已经上架。

固件上传文件为CardputerDict-0.6.4-M5Burner.bin，完整镜像0x0、ESP32-S3/8MB，与GitHub的merged.bin逐字节相同。NVS区域0x9000..0xdfff全为0xff，不含设备WiFi设置。固件和数据包不附第三方发音录音。

两者均采用同一份实际0.6.4实机UI顺序GIF：31帧、960×540、23.32秒，每帧一个完整UI，英文apple输入→释义/翻页→Tab汉英→逐字pingguo→选苹果→释义/翻页→Tab百科→逐字pingguo→选苹果→百科阅读/翻页→Tab回英汉。逐帧解码与真实截图4倍最近邻放大一致，网站上传保留原GIF动画、未使用会栅格化的图片编辑器。

公共说明与版本说明均写明：FAT32而非exFAT；完整数据约5.21GB、至少8GB建议16/32GB、至少5.3GB空闲；Release下载链接；五个维基ZIP必须全部解压到同一目录；SD根目录层次、安全插拔、首次词库缓存、完整烧录重置NVS及应用升级地址0x10000。名称和说明均以审核通过后的公开资料为准。

原始提交凭证：cardputer-submitted.png、adv-submitted.png；both-submitted-proof.png仅将两份原截图的新版卡片并排，保留版本/待审核/公开及设备分类标签。
