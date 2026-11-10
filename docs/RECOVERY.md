# 原固件恢复

2026-11-10 烧录前已保存完整 8 MB Flash，并通过 esptool verify_flash 验证与设备摘要一致。

备份文件：`/Users/huxunxun/WorkSpace/CardputerDict/backups/cardputer-before-20261110.bin`

SHA-256：`0dae80d26219dbb419c3dd28a8457a6c5f492a87c803795930f324fc2993566a`

备份只保留在本地项目的 backups/，没有放入可分享的固件或 SD 压缩包。

确需恢复时，按住 G0 重新连接 USB 进入下载模式，使用 esptool 将备份完整写回地址 0x0：

```sh
python3 /Users/huxunxun/.platformio/packages/tool-esptoolpy/esptool.py --chip esp32s3 --port /dev/cu.usbmodemXXXX --baud 460800 write_flash 0x0 /Users/huxunxun/WorkSpace/CardputerDict/backups/cardputer-before-20261110.bin
```

恢复会替换当前字典固件和 Flash 数据；不会修改独立 microSD 卡。
