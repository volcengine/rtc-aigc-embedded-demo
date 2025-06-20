# TTS burst
## 什么是TTS burst
开启该功能后，服务端生成的语音会快速发送给客户端，实现更好的抗弱网能力。

## 什么情况下可以开启TTS burst
- 该功能仅在嵌入式硬件场景下支持，且嵌入式 SDK 版本不低于1.57
- 当除了嵌入式硬件场景外，还存在 Web 端、移动端(Android/IOS)等场景，不能开启该功能

## 如何开启TTS burst
你可以通过 StartVoiceChat 接口中的 Burst 参数开启该功能, 参见 官网文档 [StartVoiceChat](https://www.volcengine.com/docs/6348/1558163)

```
    "Burst" : {
        "Enable" : true,            # 是否开启音频快速发送。默认值 false
        "BufferSize" : 500,         # 接收音频快速发送片段时，客户端可缓存的最大音频时长。注意：BufferSize 的设置需要考虑设备端的内存及网络协议栈的缓存大小。建议设置为500ms
        "Interval" : 10             # 音频快速发送结束后，其他音频内容发送间隔。取值范围为[10,600]，单位为 ms，默认值为10
    }
```
