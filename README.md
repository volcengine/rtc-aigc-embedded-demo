<h1 align="center"><img src="https://iam.volccdn.com/obj/volcengine-public/pic/volcengine-icon.png"></h1>
<h1 align="center">IoT RTC AIGC Demo</h1>
欢迎使用IoT RTC AIGC Demo，本文档为您介绍如何使用本Demo。


## 快速入门

### 前置准备
- Linux服务器，且开发环境满足Python 3.8及以上版本。
- 乐鑫 ESP32-S3-Korvo-2 或 AtomS3R 开发板。
- 参考如下流程开通硬件服务。
  - 开通火山引擎实时音视频、语音识别、音频合成、火山方舟大模型服务。参看[开通服务](https://www.volcengine.com/docs/6348/1315561)开通相关产品、配置角色策略并获取以下参数值：
    - 火山引擎 AK
    - 火山引擎 SK
    - 实时音视频应用 APPID
    - 实时音视频应用 APPKEY
    - 语音技术-语音识别-流式语音识别 APPID
    - 语音技术-音频生成-语音合成 APPID
    - 语音技术-音频生成-语音合成 Voice_type
    - 火山方舟大模型 EndPointId
  - 配置不同权限账号调用智能体, [创建角色](https://www.volcengine.com/docs/6348/1315561)
  - [启用硬件场景配置](https://console.volcengine.com/rtc/aigc/cloudRTC)，并使用相应的房间规则

### 运行服务端

> 服务端示例仅供开发者快速体验和演示，请勿在生产环境中使用。生产环境的服务端需要你自行开发。


#### 硬件要求

- PC服务器（Linux 建议使用 ubuntu18.04 及以上版本， 服务端示例程序在 Windows 11 python 3.12, MacOs python 3.9, Ubuntu 24.04 python 3.12实测可以正常运行）

#### 安装服务依赖


```shell
pip install requests
```

#### 下载并配置工程

1. 克隆实时对话式 AI 硬件 Demo 示例


    ```shell
    git clone https://github.com/volcengine/rtc-aigc-embedded-demo.git
    ```

2. 进入服务端 Demo 目录


    ```shell
    cd rtc-aigc-embedded-demo/server/src
    ```

3. 设置配置文件

    进入服务端配置文件 `rtc-aigc-embedded-demo/server/src/RtcAigcConfig.py`，设置如下参数


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

    # 服务端监听端口号,你可以根据实际业务需求设置端口号
    PORT = 8080
    ```

#### 运行服务

在 `rtc-aigc-embedded-demo/server/src`目录下运行服务


```python
python3 RtcAigcService.py
```

### 运行设备端

本文以 Mac 操作系统为例。

#### 硬件要求

- 乐鑫 ESP32-S3-Korvo-2 开发板。

- USB数据线（两条 A 转Micro-B 数据线，一条作为电源线，一条作为串口线）。

- PC（Windows、Linux 或者 macOS）。

#### 乐鑫环境配置
详见[开发环境配置文档](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/index.html)
1. 安装 CMake 和 Ninja 编译工具

    ```shell
    brew install cmake ninja dfu-util
    ```

2. 将 乐鑫 ADF 框架克隆到本地，并同步各子仓（submodule）代码
> **注意：** demo 中使用的 ADF 版本为 [eca11f20e56f9b5321b714da4305e123672d92a9], 对应 IDF 版本为 [v5.4], 请确保 ADF 版本与 IDF 版本匹配。
    1. clone 乐鑫ADF 框架

    ```shell
    git clone https://github.com/espressif/esp-adf.git // cloneADF框架
    ```
    2. 进入esp-adf目录

    ```shell
    cd esp-adf
    ```
    3. 切换到乐鑫ADF指定版本
    ```shell
    git reset --hard eca11f20e56f9b5321b714da4305e123672d92a9
    ```
    4. 同步各子仓代码

    ```shell
    git submodule update --init --recursive
    ```

3. 安装乐鑫 esp32s3 开发环境相关依赖

    ```shell
    ./install.sh esp32s3
    ```

    成功安装所有依赖后，命令行会出现如下提示

    ```shell
    All done! You can now run:
    . ./export.sh
    ```

    > 对于 macOS 用户，如在上述任何步骤中遇到以下错误:
    >
    > `<urlopen error [SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed: unable to get local issuer certificate (_ssl.c:xxx)`
    >
    > 可前往访达->应用程序->Python 文件夹，点击`Install Certificates.command` 安装证书。了解更多信息，请参考 <a target="_blank" href="https://github.com/espressif/esp-idf/issues/4775">安装 ESP-IDF 工具时出现的下载错误</a>。


4. 设置环境变量

    > **每次打开命令行窗口均需要运行该命令进行设置**

    ```shell
    . ./export.sh
    ```

#### 下载并配置工程

1. 将实时对话式 AI 硬件示例工程 clone 到 乐鑫 ADF examples 目录下
    1. 进入 esp-adf/examples 目录

    ```shell
    cd $ADF_PATH/examples
    ```
    1. clone 实时对话式 AI 硬件示例工程

    ```shell
    git clone https://github.com/volcengine/rtc-aigc-embedded-demo.git 
    ```

2. 禁用乐鑫工程中的火山组件
    1. 进入 esp-adf 目录

    ```shell
    cd $ADF_PATH
    ```
    2. 禁用乐鑫工程中的火山组件

    ```shell
    git apply $ADF_PATH/examples/rtc-aigc-embedded-demo/0001-feat-disable-volc-esp-libs.patch
    ```

#### 编译固件

1. 进入`esp-adf/examples/rtc-aigc-embedded-demo/client/espressif/esp32s3_demo` 目录下编译固件
    1. 进入 esp32s3_demo 目录

    ```shell
    cd $ADF_PATH/examples/rtc-aigc-embedded-demo/client/espressif/esp32s3_demo
    ```
    2. 设置编译目标平台

    ```shell
    idf.py set-target esp32s3
    ```
    3. 设置RTC APPID、服务端地址和端口号
    ```shell
    idf.py menuconfig
    ```
    进入 `Example Configuration` 菜单，在 `RTC APPID` 中填入你的 RTC APPID (前往 https://console.volcengine.com/rtc/listRTC 获取)，在 `AIGENT Server Host` 中填入你的服务端地址和端口号，并保存。

    4. 设置开发板型号
    ```shell
    idf.py menuconfig
    ```
    进入 `Audio HAL` 菜单，在 `Audio board` 中选择你的开发板型号。(例如: 方舟开发板选择 `M5STACK-ATOMS3R`)，并保存。

    5. 编译固件

    ```shell
    idf.py build
    ```

#### 烧录并运行示例 Demo

1. 打开乐鑫开发板电源开关

2. 烧录固件


    ```shell
    idf.py flash
    ```

3. 运行示例 Demo 并查看串口日志输出


    ```shell
    idf.py monitor
    ```
4. wifi配网
    1. 手机找到名形如“VolcRTC-XXXXXX”的wifi热点，连接上wifi。
    2. 打开浏览器，输入URL_ADDRESS 打开浏览器，输入http://192.168.4.1，进入wifi配网页面。
    3. 输入wifi名称和密码，点击提交。<br>
    注意：如果需要更换wifi名称密码，重新启动设备，设备不能在上次连上的wifi范围内，等待30s进入配网模式，重新执行上面wifi配网的3个步骤。
## 进阶阅读
[服务端示例接口说明](server/src/README.md)

## 技术交流
 欢迎加入我们的技术交流群或提出Issue，一起探讨技术，一起学习进步。
<div align=center><img src="resource/image/tech_support.png" width="200"></div>