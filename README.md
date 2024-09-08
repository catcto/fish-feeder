# fish-feeder

鱼缸自动喂食器，自动喂鱼器，使用 [ESP32S3](https://www.espressif.com/zh-hans/products/socs/esp32-s3) 开发板 和 GA12-N20 减速电机，支持定时和定量喂食, 使用 [Home Assistant](https://www.home-assistant.io/) 控制。

## 视频演示

## 3D 模型

使用 Blender 软件建模，拓竹 A1 打印机， STL 文件在 [3d-models](3d-models) 目录。



## 开发环境

使用 Arduino 和 PlatformIO 开发

### 编译上传固件

```shell
$ # 初始化项目
$ pio project init --board esp32-s3-devkitc-1 --project-option "framework=arduino"

$ # 查看设备，自行修改 platformio.ini 文件中的上传和监视端口
$ pio device list

$ # 编译上传并监视串口输出
$ pio run --target upload && platformio device monitor
```
