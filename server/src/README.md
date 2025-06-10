# 服务端示例接口说明

1. http 请求头说明

    ```shell
    # headers
    # Content-Type 固定值 application/json
    # Authorization af78e30 + ${CONFIG_RTC_APPID} 
    ```

2. 启动智能体
- 请求示例
    ```shell
    curl --location 'http://127.0.0.1:8080/startvoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: af78e3067******' \
    --data '{"end_point_id":"ep-2024*****", "audio_codec":"G711A"}'
    ```

- 请求体说明
    ```shell
    # 请求体是一个 json 字符串

    # audio_codec 
    # 非必填，字符串，默认值 G711A
    # 和智能体对话使用的音频传输格式，支持G711A，G722，OPUS，AAC。

    # end_point_id 
    # 非必填，字符串，默认值 ep-20250122160517-hlnzt
    # 非必填，对话的智能体的end point id

    # asr_type 
    # 非必填，整数，默认值 0
    # 语音识别类型
    # 0 小模型 ASR
    # 1 大模型 ASR 小时版-流式输入流式输出
    # 2 大模型 ASR 并发版-流式输入流式输出
    # 3 大模型 ASR 小时版-流式输入非流式输出
    # 4 大模型 ASR 并发版-流式输入非流式输出

    # interrupt_speech_duration
    # 非必填，整数，默认值 0
    # 自动打断触发阈值。房间内真人用户持续说话时间达到该参数设定值后，智能体自动停止输出。取值范围为0，[200，3000]，单位为 ms，值越大智能体说话越不容易被打断。默认值为 0，表示用户发出声音且包含真实语义时即打断智能体输出。

    # vad_silence_time
    # 非必填，整数，默认值 0
    # 人声检查判停时间。停顿时间若高于该值设定时间，则认为一句话结束。取值范围为 [500，3000)，单位为 ms，默认值为 600

    # tts_is_bidirection
    # 非必填，布尔值，默认值 false
    # 是否是双向流式语音合成

    # voice_type
    # 非必填，字符串，默认值 BV007_streaming
    # 语音合成的声音音色类型

    # llm_prefill
    # 非必填，布尔值，默认值 false
    # 将 ASR 中间结果提前送入大模型进行处理以降低延时。开启后会产生额外模型消耗。

    # disable_rts_subtitle
    # 非必填，布尔值，默认值 false
    # 禁用rts字幕回调

    # enable_conversation_state_callback
    # 非必填，布尔值，默认值 false
    # 是否接收任务状态变化回调

    # enable_burst
    # 非必填，布尔值，默认值 false
    # 音频快速发送配置。开启该功能后，可通过快速发送音频实现更好的抗弱网能力。

    # burst_buffer_size
    # 非必填，整数，默认值 500
    # 接收音频快速发送片段时，客户端可缓存的最大音频时长。取值范围为[10,3600000]，单位为 ms

    # burst_interval
    # 非必填，整数，默认值 20
    # 音频快速发送结束后，其他音频内容发送间隔。取值范围为[10,600]，单位为 ms

    # fc_tools
    # 非必填， json数组，默认值 []
    # function call 工具列表，格式参考 https://www.volcengine.com/docs/6348/1359441

    ```
- 返回示例及说明
    ```json
    // 成功返回示例
    {
        "code": 200,
        "msg": "",
        "data": {
            "room_id": "G711Ad2ae*****",
            "uid": "userd2ae*****",
            "app_id": "67*****",
            "token": "00167*****CzVoPW/3AhM8*****T4bQ==",
            "task_id": "d2ae*****",
            "bot_uid": "botd2ae*****"
        }
    }

    // 失败返回示例
    {
        "code": 400,
        "msg": "header Authorization error, Bad Authorization."
    }
    ```

    ```bash
    # code： 状态码 只有200为成功，其他为失败
    # msg： 其它状态码的错误提示信息
    
    # data： 房间信息
    # room_id： rtc 房间 id
    # uid： rtc 客户端用户 id
    # app_id： rtc app id
    # token： rtc 加入房间的鉴权 token
    # task_id： 智能体任务 id
    # bot_uid： 智能体用户 id
    ```
3. 更新智能体体
- 请求示例
    ```shell
    curl --location 'http://127.0.0.1:8080/updatevoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: af78e3067*****' \
    --data '{
        "app_id": "af78e3067******",
        "room_id": "G711Abf4106*****",
        "task_id": "bf4106*****",
        "command": "function",
        "message": "{\"subscriber_user_id\":\"\",\"tool_calls\":[{\"function\":{\"arguments\":\"{\\\"location\\\": \\\"\\u5317\\u4eac\\u5e02\\\"}\",\"name\":\"get_current_weather\"},\"id\":\"call_py400kek0*****\",\"type\":\"function\"}]}"
    }'

    ```
- 请求体说明
    ```shell
    # app_id： rtc app id

    # room_id： rtc 房间 id

    # task_id： 智能体任务 id

    # command： 命令类型，目前支持 function, interrupt, external_text_to_speech, external_prompts_for_llm, external_text_to_llm, finish_speech_recognition
    #           参考：https://www.volcengine.com/docs/6348/1404671  除了 function 命令外，其它命令的message会直接透传给open api
    #           function: function calling 命令, message 需要传入客户端的 function calling 消息，服务端会做一个假的处理
    #           interrupt: 打断命令，使用此命令不需要填 message
    #           external_text_to_speech: 传入文本信息供 TTS 音频播放，使用此命令时必须提供 message
    #           external_prompts_for_llm: 传入自定义文本结合用户问题送入 LLM，使用此命令时必须提供 message    
    #           external_text_to_llm: 传入外部问题送入LLM，使用此命令时必须提供 message
    #           finish_speech_recognition: 触发新一轮对话，使用此命令不需要 message

    # message： 命令消息，参考 command 的说明

    # interrupt_mode: 传入文本信息或外部问题时，处理的优先级。
    #                 当 command 为 ExternalTextToSpeech 或 ExternalTextToLLM 时为该参数必填。
    #                 1：高优先级。传入信息直接打断交互，进行处理。
    #                 2：中优先级。等待当前交互结束后，进行处理。
    #                 3：低优先级。如当前正在发生交互，直接丢弃 Message 传入的信息。

    ```
- 返回示例及说明
    ```json
    // 成功返回示例
    {
        "code": 200,
        "msg": "",
        "data": {
            "app_id": "66b****",
            "room_id": "G711Abf4*****",
            "task_id": "bf4*****",
            "command": "function",
            "message": "{\"subscriber_user_id\":\"\",\"tool_calls\":[{\"function\":{\"arguments\":\"{\\\"location\\\": \\\"\北\京\市\\\"}\",\"name\":\"get_current_weather\"},\"id\":\"call_py400ke*****\",\"type\":\"function\"}]}"
        }
    }
    // 失败返回示例
    {
        "code": 400,
        "msg": "update_voice_chat: your command == function, \"message\" must be in json"
    }
    ```
    
4. 停止智能体
- 请求示例
    ```shell
    curl --location 'http://127.0.0.1:8080/stopvoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: af78e30675*****' \
    --data '{
        "app_id": "66b*****",
        "room_id": "G711Abf4*****",
        "task_id": "bf4*****"
    }'
    ```
- 请求体说明
    ```shell
    # app_id： rtc app id

    # room_id： rtc 房间 id 

    # task_id： 智能体任务 id
    ```
- 返回示例及说明
    ```json
    // 成功返回示例
    {
        "code": 200,
        "msg": "",
        "data": {
            "app_id": "66b*****",
            "room_id": "G711Abf4*****",
            "task_id": "bf4*****"
        }
    }

    // 失败返回示例
    {
        "code": 400,
        "msg": "header Authorization error, Bad Authorization."
    }
    ```
