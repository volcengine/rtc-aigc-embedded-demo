<h1 align="center"><img src="https://iam.volccdn.com/obj/volcengine-public/pic/volcengine-icon.png"></h1>
<h1 align="center">IOT RTC AIGC Demo</h1> 
欢迎使用IOT RTC AIGC Demo，本文档为您介绍如何使用本Demo。


# 服务端
    服务端demo使用python3实现的，在性能和安全性没有做任何事情，用于帮助您快速上手，跑通全部流程。代码仅供参考，您在实际使用中需要考虑并发性能和安全性。

    服务端程序不是必要的，可以将服务端的部分逻辑内置到客户端实现。如果您需要放在客户端实现，请一定要保障SK，AK，RTC_APP_KEY的安全，避免被任何第三方获取到。
## 前置准备
### 服务开通
请您仔细阅读，并按照[开通服务文档](https://www.volcengine.com/docs/6348/1315561)开通所需服务，并获取AK(Access Key Id), SK(Secret Access Key), RTC_APP_ID(实时音视频app id), RTC_APP_KEY(实时音视频app key), DEFAULT_BOT_ID(大模型接入点id), DEFAULT_VOICE_ID(音色类型), ASR_APP_ID(语音识别服务app id), TTS_APP_ID(语音转文本服务app id) 并填入server/src/RtcAigcConfig.py中。

<b style="color:red">特别提醒：请保护好您的SK，AK 和 RTC_APP_KEY，否则可能会造成您以及您的客户的信息泄露风险，还可能会造成账户财产损失。</b>

```
SK = "Tm1FN************"
AK = "AKLTY************"

RTC_APP_ID = "e671fa53******"
RTC_APP_KEY = "62a3081******"

DEFAULT_BOT_ID = "ep-2025********"
DEFAULT_VOICE_ID = "zh_female****"

ASR_APP_ID = "64410*****"
TTS_APP_ID = "64410*****"

// 服务端口，根据需要设置
PORT = 8080
```

### 申请加入策略组
Demo 目前仅支持G711A编码，需要申请加入策略组。请发送邮件到zhouhuichao@bytedance.com
标题：“申请加入LLM策略组”
内容：您的RTC_APP_ID，此处支持多个RTC_APP_ID
加入完成后通过邮件回复。

### 运行环境
- python3
- python3 requests

### 启动服务
```
# 在server/src 目录运行
python3 RtcAigcService.
```

# 客户端

## 乐鑫
### esp32s3
#### 前置准备
1. 请您参考乐鑫平台 [开发环境配置文档](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/index.html)，配置好开发环境，准备好开发工具

2. 填写client\espressif\esp32s3_demo\main\Config.h

    ```
    // 服务端的地址
    #define DEFAULT_SERVER_HOST "127.0.0.1:8080"

    // 默认的智能体id
    #define DEFAULT_BOT_ID "ep-20240729********"

    // 默认声音id
    #define DEFAULT_VOICE_ID "zh_female_*******"

    // RTC APP ID
    #define DEFAULT_RTC_APP_ID "5c833ef********"
    ```
3. 从火山引擎官网获取VolcEngineRTCLite库放置到client/espressif/esp32s3_demo/components/目录下，目录结构如下：
```
VolcEngineRTCLite
├── CMakeLists.txt
├── idf_component.yml
├── include
│   └── VolcEngineRTCLite.h
└── libs
    └── esp32s3
        └── libVolcEngineRTCLite.a
```
#### 使用方法
1. 复制 client/espressif/esp32s3_demo 到 esp-adf工程的examples目录下
2. 在乐鑫的ESP-IDF控制台中在执行下列命令
    ```
    # 工作目录 esp-adf工程的examples/esp32s3_demo
    idf.py fullclean
    idf.py set-target esp32s3
    idf.py build
    idf.py flash
    ```

