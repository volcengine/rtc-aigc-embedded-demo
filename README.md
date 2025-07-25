<h1 align="center"><img src="https://iam.volccdn.com/obj/volcengine-public/pic/volcengine-icon.png"></h1>
<h1 align="center">IoT RTC AIGC Demo</h1>

欢迎使用IoT RTC AIGC Demo，本文档为您介绍如何使用本 Demo，快速在嵌入式硬件设备中，打造一个端到端的 AI 语音助手。
通过本 Demo，你可以体验到：
- 低延迟 AI 对话：基于 RTC 打造的低延迟语音交互链路。
- 端云一体：设备端负责音频采集、唤醒和播放，云端负责复杂的 ASR、LLM 和 TTS 处理。


## 快速接入

### 步骤一：前置准备

1. 开通火山引擎 **实时音视频**、**语音识别**、**音频合成**、**火山方舟大模型** 服务。参看[开通服务](https://www.volcengine.com/docs/6348/1315561)开通相关产品、配置角色策略并获取以下参数值：
    - 火山引擎：**AK**、**SK**
    - 实时音视频 RTC：**APPID**、**APPKEY**
    - 豆包语音-语音识别-流式语音识别：**APP ID**、**Access Token**
    - 豆包语音-音频生成-语音合成：**APP ID**、**Access Token**、**Voice_type**
    - 火山方舟大模型：**EndPointId**
2. 配置不同权限账号调用智能体，[创建角色](https://www.volcengine.com/docs/6348/1315561)。
3. [启用硬件场景配置](https://console.volcengine.com/rtc/aigc/cloudRTC)，并使用相应的房间规则。
4. 准备 Linux PC 服务器：推荐使用 Ubuntu18.04 及以上版本。（服务端示例程序在 Windows 11 python 3.12、MacOs python 3.9、Ubuntu 24.04 Python 3.12 实测可以正常运行。）

### 步骤二：运行服务端

> **注意**: 服务端示例仅供开发者快速体验和演示，请勿在生产环境中使用。生产环境的服务端需要你自行开发。


#### 硬件要求

- PC服务器（Linux 建议使用 ubuntu18.04 及以上版本， 服务端示例程序在 Windows 11 python 3.12, MacOs python 3.9, Ubuntu 24.04 python 3.12实测可以正常运行）

#### 安装服务依赖
```bash
pip3 install requests
```
#### 下载并配置工程
1. 克隆实时对话式 AI 硬件 Demo 示例。
    ```bash
    git clone https://github.com/volcengine/rtc-aigc-embedded-demo.git
    ```
2. **进入服务端 Demo 目录**。
    ```bash
    cd rtc-aigc-embedded-demo/server/src
    ```
3. **设置配置文件**
    进入服务端配置文件 `rtc-aigc-embedded-demo/server/src/RtcAigcConfig.py`，设置如下参数（**请务必使用你在准备工作获取的值**）：
    ```python
    # 鉴权 AK/SK。前往 https://console.volcengine.com/iam/keymanage 获取
    SK = "WmpCbVl6Y3hOR1JrT************1tTTRZalF4WW1FeE56WQ=="
    AK = "AKLTNWQyODQ1MDM5Y***********WRmM2Y2NTJlMTQyZjI"

    # 实时音视频 App ID。前往 https://console.volcengine.com/rtc/listRTC 获取或创建
    CONFIG_RTC_APPID = "67582ac8******0174410bd1"
    # 实时音视频 APP KEY。前往 https://console.volcengine.com/rtc/listRTC 获取
    RTC_APP_KEY = "1a6a03723c******222ada877ee13b"

    # 大模型推理接入点 EndPointId 前往 https://console.volcengine.com/ark/region:ark+cn-beijing/endpoint?config=%7B%7D 创建
    DEFAULT_END_POINT_ID = "ep-2025******160517-hlnzt"
    # 音频生成-语音合成 Voice_type，前往 https://console.volcengine.com/speech/service/8 获取
    DEFAULT_VOICE_TYPE = "BV007_******ming"

    # 语音识别-流式语音识别 APPID 前往 https://console.volcengine.com/speech/service/16 获取
    ASR_APP_ID = "884***621"
    # 语音识别-流式语音识别 ACCESS TOKEN 前往 https://console.volcengine.com/speech/service/16 获取
    ASR_ACCESS_TOKEN = "M_X6X***BeXa1"

    # 音频生成-语音合成 APPID，前往 https://console.volcengine.com/speech/service/8 获取
    TTS_APP_ID = "884***9621"
    # 音频生成-语音合成 ACCESS TOKEN，前往 https://console.volcengine.com/speech/service/8 获取
    TTS_ACCESS_TOKEN = "M_X6X***BeXa1"

    # 服务端监听端口号，你可以根据实际业务需求设置端口号
    PORT = 8080
    ```
#### 运行服务

在 `rtc-aigc-embedded-demo/server/src` 目录下运行服务。

```bash
python3 RtcAigcService.py
```


### 运行设备端

请根据你使用的硬件开发板，选择对应的设备端部署教程：

- 乐鑫 ESP32-S3-Korvo-2、AtomS3R：[运行设备端_乐鑫](docs/QUICK_START_ESP.md)
- 博通：[运行设备端_博通](docs/QUICK_START_BEKEN.md)



## 进阶阅读
- [服务端示例接口说明](server/src/README.md)
- [开启 TTS burst 功能](docs/TTS_BURST.md)

## 技术交流
 欢迎加入我们的技术交流群或提出Issue，一起探讨技术，一起学习进步。
<div align=center><img src="resource/image/tech_support.png" width="200"></div>