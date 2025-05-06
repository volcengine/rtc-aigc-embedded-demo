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
  - 演示示例需要申请加入策略组。Demo目前仅支持G711A编码，需要发送邮件到[Conversational_AI@bytedance.com]，主题：“申请加入LLM策略组”内容：您的RTC_APP_ID。可申请多个RTC_APP_ID。如果自行实现OPUS编码，可以直接使用，无需加白

### 运行服务端

> 服务端示例仅供开发者快速体验和演示，请勿在生产环境中使用。生产环境的服务端需要你自行开发。


#### 硬件要求

- PC服务器（Linux 建议使用 ubuntu18.04 及以上版本）

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
    AK = "yzitS6Kx0x** ***fo08eYmYMhuTu"
    SK = "xZN65nz0CFZ** ****lWcAGsQPqmk"

    # 实时音视频 App ID。前往 https://console.volcengine.com/rtc/listRTC 获取或创建
    RTC_APP_ID = "678e1574** ***b9389357"
    # 实时音视频 APP KEY。前往 https://console.volcengine.com/rtc/listRTC 获取
    RTC_APP_KEY = "dc7f8939d23** *****bacf4a329"

    # 大模型推理接入点 EndPointId 前往 https://console.volcengine.com/ark/region:ark+cn-beijing/endpoint?config=%7B%7D 创建
    DEFAULT_BOT_ID = "ep-202** ****36-plsp5"
    # 音频生成-语音合成 Voice_type，前往 https://console.volcengine.com/speech/service/8 获取
    DEFAULT_VOICE_ID = "BV05** ****aming"

    # 语音识别-流式语音识别 APPID 前往 https://console.volcengine.com/speech/service/16 获取
    ASR_APP_ID = "274** **256"
    # 音频生成-语音合成 APPID，前往 https://console.volcengine.com/speech/service/8 获取
    TTS_APP_ID = "274** **256"

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
> **注意：** demo 中使用的 ADF 版本为 [0d76650198ca96546c40d10a7ce8963bacdf820b], 对应 IDF 版本为 [v5.4], 请确保 ADF 版本与 IDF 版本匹配。
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
    git reset --hard 0d76650198ca96546c40d10a7ce8963bacdf820b
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

2. 打开设备端配置文件 `rtc-aigc-embedded-demo/client/espressif/esp32s3_demo/main/Config.h`，设置如下参数


    ```c
    // 你的服务端地址:监听端口号
    #define DEFAULT_SERVER_HOST "127.0.0.1:8080"

    // 服务端设置的大模型 EndPointId
    #define DEFAULT_BOT_ID "ep-20240729** **** **"

    // 服务端设置的音频生成-语音合成 Voice_type
    #define DEFAULT_VOICE_ID "zh_female_** *****"

    // 服务端设置的实时音视频 APPID
    #define DEFAULT_RTC_APP_ID "5c833ef** **** **"
    ```

3. 禁用乐鑫工程中的火山组件
    1. 进入 esp-adf 目录

    ```shell
    cd $ADF_PATH
    ```
    2. 禁用乐鑫工程中的火山组件

    ```shell
    git apply $ADF_PATH/examples/rtc-aigc-embedded-demo/0001-fix-disable-volc-engine-in-esp.patch
    ```

    3. 更新AtomS3R开发板补丁
    ```shell
    git apply $ADF_PATH/examples/rtc-aigc-embedded-demo/0001-add-atoms3r-board.patch
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
    3. 设置WIFI账号密码
    ```shell
    idf.py menuconfig
    ```
    进入 `Example Connection Configuration` 菜单，在 `WiFi SSID` 及 `WiFi Password` 中填入你的 WIFI 账号和密码，并保存。

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

### 技术交流
 欢迎加入我们的技术交流群或提出Issue，一起探讨技术，一起学习进步。
<div align=center><img src="resource/image/tech_support.png" width="200"></div>