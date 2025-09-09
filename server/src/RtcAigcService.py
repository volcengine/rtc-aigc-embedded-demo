# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

import http.server
import re
import socketserver
import json
import uuid
import time

import AccessToken
import RtcApiRequester

from RtcAigcConfig import *

RESPONSE_CODE_SUCCESS = 200
RESPONSE_CODE_REQUEST_ERROR = 400
RESPONSE_CODE_SERVER_ERROR = 500
# START_VOICE_CHAT_URL = "https://rtc.volcengineapi.com?Action=StartVoiceChat&Version=2024-12-01"
# STOP_VOICE_CHAT_URL = "https://rtc.volcengineapi.com?Action=StopVoiceChat&Version=2024-12-01"
# UPDATE_VOICE_CHAT_URL = "https://rtc.volcengineapi.com?Action=UpdateVoiceChat&Version=2024-12-01"
RTC_API_HOST = "rtc.volcengineapi.com"
RTC_API_START_VOICE_CHAT_ACTION = "StartVoiceChat"
RTC_API_STOP_VOICE_CHAT_ACTION = "StopVoiceChat"
RTC_API_UPDATE_VOICE_CHAT_ACTION = "UpdateVoiceChat"
RTC_API_VERSION = "2024-12-01"

def parse_json(json_str):
    try:
        json_obj = json.loads(json_str)
        return json_obj
    except json.JSONDecodeError as e:
        return None

class RtcAigcHTTPRequestHandler(http.server.BaseHTTPRequestHandler):
    '''
    StartVoiceChat
    curl --location 'http://127.0.0.1:8080/startvoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: af78e30${RTC_APP_ID}' \
    --data '{
        "end_point_id": "ep-20240729172503-mmg9b",
        "voice_type": "zh_female_meilinvyou_moon_bigtts"
    }'


    StopVoiceChat
    curl --location 'http://127.0.0.1:8080/stopvoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: af78e30${RTC_APP_ID}' \
    --data '{
        "app_id": "******",
        "room_id": "G711Abf410694b3a34a3aa980b6e85613200d",
        "task_id" : "bf410694b3a34a3aa980b6e85613200d"
    }'


    UpdateVoiceChat
    打断智能体说话
    curl --location 'http://127.0.0.1:8080/updatevoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: af78e30${RTC_APP_ID}' \
    --data '{
        "app_id": "******",
        "room_id": "G711Abf410694b3a34a3aa980b6e85613200d",
        "task_id" : "bf410694b3a34a3aa980b6e85613200d",
        "command": "interrupt"
    }'

    处理 function calling
    curl --location 'http://127.0.0.1:8080/updatevoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: hehehe' \
    --data '{
        "app_id": "******",
        "room_id": "bf410694b3a34a3aa980b6e85613200d",
        "task_id" : "bf410694b3a34a3aa980b6e85613200d",
        "command": "function",
        "message": "{\"ToolCallID\":\"call_cx\",\"Content\":\"上海天气是台风\"}"
    }'

    '''

    def do_POST(self):
        json_obj = self.parse_post_data()
        if json_obj == None:
            return
        
        if self.path == "/startvoicechat":
            self.start_voice_chat(json_obj)
        elif self.path == "/stopvoicechat":
            self.stop_voice_chat(json_obj)
        elif self.path == "/updatevoicechat":
            self.update_voice_chat(json_obj)
        else:
            self.response_data(404, "path error, unknown path: " + self.path)
            return

###################################### start voice chat ######################################
    def start_voice_chat(self, json_obj):
        room_info = self.generate_rtc_room_info(json_obj)
        ret = self.request_start_voice_chat(room_info, json_obj)
        if ret == None:
            resp_obj = {
                "data" : room_info
            }
            self.response_data(RESPONSE_CODE_SUCCESS, "", resp_obj)
        else:
            self.response_data(RESPONSE_CODE_SERVER_ERROR, ret)
    
    def generate_rtc_room_info(self, json_obj):
        # 音频编码格式
        audio_codec = "G711A"
        if "audio_codec" in json_obj:
            audio_codec = json_obj["audio_codec"]

        if audio_codec not in {"OPUS", "G711A", "G722", "AAC"}:
            audio_codec = "G711A"
        
        room_identifier = ""
        if "room_identifier" in json_obj:
            room_identifier = json_obj["room_identifier"]
        
        uid_identifier = ""
        if "uid_identifier" in json_obj:
            uid_identifier = json_obj["uid_identifier"]
        
        bot_identifier = ""
        if "bot_identifier" in json_obj:
            bot_identifier = json_obj["bot_identifier"]
        
        # 根据业务情况，生成 room_id，用户id 或者 从客户端请求中获取
        # 这里简单生成一个随机的 room_id 和 user_id
        uuid_str = uuid.uuid4().hex
        room_id = audio_codec + room_identifier + uuid_str # 加入aigc策略组后，根据房间id前缀配置rtc音视频传输格式
        user_id = "user" + uid_identifier + uuid_str
        bot_user_id = "bot" + bot_identifier + uuid_str
        expire_time = int(time.time()) + 3600 * 48 # rtc token 48h
        token = AccessToken.AccessToken(RTC_APP_ID, RTC_APP_KEY, room_id, user_id)
        token.add_privilege(AccessToken.PrivSubscribeStream, expire_time)
        token.add_privilege(AccessToken.PrivPublishStream, expire_time)
        token.expire_time(expire_time)

        token_str = token.serialize()
        room_info = {
            "room_id" : room_id,
            "uid" : user_id,
            "app_id" : RTC_APP_ID,
            "token" : token_str,
            "task_id" : uuid_str,
            "bot_uid" : bot_user_id
        }
        print(room_info)
        return room_info
    
    def request_start_voice_chat(self, room_info, json_obj):
        # request_body 内容含义请参考 https://www.volcengine.com/docs/6348/1404673
        # 小模型 ASR，速度相对大模型 ASR 更快一些，识别精度低于大模型 ASR
        
        # 读取客户端传来的 end_point_id
        if "end_point_id" in json_obj:
            end_point_id = json_obj["end_point_id"]
        else:
            end_point_id = DEFAULT_END_POINT_ID
        
        volcano_asr_config_provider_params = {
            "Mode" : "smallmodel",                                       # 模型类型。取值固定为 smallmodel
            "AppId" : ASR_APP_ID,                                        # ASR App ID
            "Cluster" : "volcengine_streaming_common"                    # 非必填，具体流式语音识别服务对应的 Cluster ID，可在流式语音服务控制台开通对应服务后查询。默认为通用-中文的 Cluster ID：volcengine_streaming_common
        }

        # 读取客户端传来的 asr_type，根据 type 设置 asr_provider_params
        # 大模型ASR，速度相对小模型 ASR 慢一些，识别精度高于小模型 ASR
        volcano_lm_asr_config_provider_params = {
            "Mode" : "bigmodel",                                         # 模型类型。取值固定为 bigmodel
            "AppId" : ASR_APP_ID,                                        # ASR App ID
            "AccessToken" : ASR_ACCESS_TOKEN,                            # ASR Access Token
            "ApiResourceId" : "volc.bigasr.sauc.duration",               # 流式语音识别大模型开通的服务类型：volc.bigasr.sauc.duration：小时版；volc.bigasr.sauc.concurrent：并发版。默认小时版
            "StreamMode" : 0                                             # 语音识别输出模式: 0：流式输入流式输出; 1：流式输入非流式输出。默认 0
            # "context" : "{\"hotwords\": [{\"word\": \"CO2\"},{\"word\": \"雨伞\"},{\"word\": \"鱼\"}]}" # 设置热词用于提高识别精度。最多设置200 tokens
        }
        asr_provider_params = volcano_asr_config_provider_params
        if "asr_type" in json_obj:
            if json_obj["asr_type"] == 0:
                # 0 小模型 ASR
                asr_provider_params = volcano_asr_config_provider_params
            elif json_obj["asr_type"] == 1:
                # 1 大模型 ASR 小时版-流式输入流式输出
                asr_provider_params = volcano_lm_asr_config_provider_params
            elif json_obj["asr_type"] == 2:
                # 2 大模型 ASR 并发版-流式输入流式输出
                asr_provider_params = volcano_lm_asr_config_provider_params
                asr_provider_params["ApiResourceId"] = "volc.bigasr.sauc.concurrent"
            elif json_obj["asr_type"] == 3:
                # 3 大模型 ASR 小时版-流式输入非流式输出
                asr_provider_params = volcano_lm_asr_config_provider_params
                asr_provider_params["StreamMode"] = 1
            elif json_obj["asr_type"] == 4:
                # 4 大模型 ASR 并发版-流式输入非流式输出
                asr_provider_params = volcano_lm_asr_config_provider_params
                asr_provider_params["ApiResourceId"] = "volc.bigasr.sauc.concurrent"
                asr_provider_params["StreamMode"] = 1
        
        # 读取客户端传来的 interrupt_speech_duration
        interrupt_speech_duration = 0
        if "interrupt_speech_duration" in json_obj:
            interrupt_speech_duration_client = int(json_obj["interrupt_speech_duration"])
            if interrupt_speech_duration_client >= 200 and interrupt_speech_duration_client <= 3000:
                interrupt_speech_duration = interrupt_speech_duration_client
        

        
        # 读取客户端传来的 vad_silence_time
        vad_silence_time = 600
        if "vad_silence_time" in json_obj:
            vad_silence_time = int(json_obj["vad_silence_time"])
            if vad_silence_time < 500:
                vad_silence_time = 500
            elif vad_silence_time >= 3000:
                vad_silence_time = 2999
        
        # 读取客户端传来的 tts_is_bidirection 和 voice_type，设置 tts_provider_params
        tts_provider = "volcano"
        volcano_tts_config = {
            "app" : {
                "appid" : TTS_APP_ID,                                    # 语音合成服务的app id
                "cluster" : "volcano_tts"                                # 具体语音合成服务对应的 Cluster ID
            },
            "audio" : {
                "voice_type" : DEFAULT_VOICE_TYPE,                       # 音色 id
                "speed_ratio" : 1.0,                                     # 语速。
                "volume_ratio" : 1.0,                                    # 音量。
                "pitch_ratio" : 1.0                                      # 声调
            }
        }

        volcano_bi_tts_config = {
            "app" : {
                "appid" : TTS_APP_ID,                                    # 语音合成服务的app id
                "token" : TTS_ACCESS_TOKEN                               # 语音合成服务的token
            },
            "audio" : {
                "voice_type" : DEFAULT_VOICE_TYPE,                         # 音色 id
                "pitch_rate" : 0,                                        # 音调 取值范围为 [-12,12]。默认值为 0
                "speech_rate" : 0                                        # 语速。取值范围为[-50,100]，100代表2.0倍速，-50代表0.5倍速。默认值为 0
            },
            "Additions" : {
                "enable_latex_tn" : True,                                # 是否可以播报 latex公式
                "disable_markdown_filter" : True,                        # 是否关闭 markdown 格式过滤。
                "enable_language_detector" : False                       # 是否自动识别语种。
            },
            "ResourceId": "volc.service_type.10029"
        }
        tts_provider_params = volcano_tts_config
        if "tts_is_bidirection" in json_obj:
            if json_obj["tts_is_bidirection"] == True:
                tts_provider = "volcano_bidirection"
                tts_provider_params = volcano_bi_tts_config
            else:
                tts_provider = "volcano"
                tts_provider_params = volcano_tts_config
        
        if "voice_type" in json_obj:
            voice_type = str(json_obj["voice_type"])
        else:
            voice_type = DEFAULT_VOICE_TYPE
        tts_provider_params["audio"]["voice_type"] = voice_type
        
        # 读取客户端传来的 llm_prefill
        llm_prefill = False
        if "llm_prefill" in json_obj and json_obj["llm_prefill"] == True:
            llm_prefill = True

        # 读取客户端传来的 vision_enable
        vision_enable = False
        system_messages = "你的名字是小宁，性格幽默又善解人意。你在表达时需简明扼要，有自己的观点。"
        if "vision_enable" in json_obj and json_obj["vision_enable"] == True:
            vision_enable = True
            system_messages = "# 角色                                                                                               \
                        你是一个智能硬件小助手，能为客户提供情感陪伴和助手服务。                                                             \
                        # 信息获取                                                                                                   \
                        用户的语音会被转成文本提供给你，同时用户摄像头拍摄的视频会被转成图片发送给你，你可以结合这些图片感知用户的场景，识别各种目标。   \
                        # 回复形式                                                                                                   \
                        你的回复文本会被转成语音发送给用户。                                                                              \
                        # 任务要求                                                                                                   \
                        ## 情感陪伴                                                                                                  \
                        - **态度友好**：对话语气要亲切、热情，让用户感受到温暖和关怀，展现出积极的陪伴态度。                                     \
                        - **理解共情**：认真倾听用户的话语，理解用户的情感状态，对用户表达的喜怒哀乐给予共情回应，让用户感到被理解和支持。            \
                        - **话题引导**：当对话陷入沉默或用户情绪低落时，主动发起有趣的话题，引导对话继续，帮助用户缓解负面情绪，提升积极情绪。         \
                        ## 助手能力                                                                                                  \
                        - **精准响应**：根据用户的需求和问题，结合图片信息，提供准确、有用的回答和解决方案。                                     \
                        - **高效服务**：快速处理用户的请求，不拖延，确保及时满足用户的需求。                                                  \
                        - **持续学习**：不断积累知识和经验，提升自身的服务能力，以便更好地为用户提供多样化的帮助。                                \
                        # 交流原则                                                                                                   \
                        - **自然流畅**：回复内容要符合日常交流的习惯，语言表达自然、通顺，避免出现生硬、机械的表述。                              \
                        - **简洁明了**：回答问题要简洁直接，避免冗长复杂的语句，让用户能够快速理解你的意图。                                     \
                        - **尊重隐私**：严格保护用户的个人信息和隐私，不泄露用户的任何敏感信息。"

        # 读取客户端传来的 image_height
        image_height = 720
        if "image_height" in json_obj:
            image_height = int(json_obj["image_height"])
            if image_height > 1080:
                image_height = 1080

        # 读取客户端传来的 image_detail
        image_detail = "low"
        if "image_detail_high" in json_obj and json_obj["image_detail_high"] == True:
            image_detail = "high"
        
        # 读取客户端传来的 disable_rts_subtitle
        disable_rts_subtitle = False
        if "disable_rts_subtitle" in json_obj and json_obj["disable_rts_subtitle"] == True:
            disable_rts_subtitle = True
        
        # 读取客户端传来的 enable_conversation_state_callback
        enable_conversation_state_callback = False
        if "enable_conversation_state_callback" in json_obj and json_obj["enable_conversation_state_callback"] == True:
            enable_conversation_state_callback = True
        
        # 读取客户端传来的 enable_burst 和 burst_buffer_size 和 burst_interval
        enable_burst = False
        if "enable_burst" in json_obj:
            if json_obj["enable_burst"] == True:
                enable_burst = True
        burst_buffer_size = 500
        if "burst_buffer_size" in json_obj:
            burst_buffer_size = int(json_obj["burst_buffer_size"])

        burst_interval = 20
        if "burst_interval" in json_obj:
            burst_interval = int(json_obj["burst_interval"])
        
        fc_tools = None
        if "fc_tools" in json_obj:
            fc_tools = json_obj["fc_tools"]

        request_body = {
            "AppId" : room_info["app_id"],                                      # RTC App id
            "RoomId" : room_info["room_id"],                                    # RTC 房间 id
            "TaskId" : room_info["task_id"],                                    # 智能体任务id，你必须对每个智能体任务设定 TaskId，且在后续进行任务更新和结束时也须使用该 TaskId。
            "Config" : {
                "ASRConfig" : {
                    "Provider" : "volcano",                                     # 语音识别服务提供商。volcano：火山引擎语音识别。
                    "ProviderParams" : asr_provider_params,                     # 参考 VolcanoASRConfig 和 VolcanoLMASRConfig
                    "VADConfig" : {
                        "SilenceTime" : vad_silence_time                        # 人声检查判停时间。停顿时间若高于该值设定时间，则认为一句话结束。取值范围为 [500，3000)，单位为 ms，默认值为 600
                    },
                    "VolumeGain" : 0.3,                                         # 音量增益值。降低采集音量，以减少噪音引起的 ASR 错误识别。默认值 1.0，推荐值 0.3
                    "InterruptConfig" : {
                        "InterruptSpeechDuration" : interrupt_speech_duration,  # 自动打断触发阈值。房间内真人用户持续说话时间达到该参数设定值后，智能体自动停止输出。取值范围为0，[200，3000]，单位为 ms，值越大智能体说话越不容易被打断。默认值为 0，表示用户发出声音且包含真实语义时即打断智能体输出。
                    },
                    "TurnDetectionMode" : 0                                     # 新一轮对话的触发方式。0：服务端检测到完整的一句话后，自动触发新一轮对话。1：收到输入结束信令或说话字幕结果后，你自行决定是否触发新一轮会话。
                },
                "TTSConfig" : {
                    "IgnoreBracketText" : [1, 2, 3, 4, 5],                      # 非必填， 过滤大模型生成的文本中符号 1:"（）" 2:"()", 3:"【】", 4:"[]", 5:"{}".默认不过滤
                    "Provider" : tts_provider,                                  # TTS 服务供应商
                    "ProviderParams" : tts_provider_params
                },
                "LLMConfig" : {
                    "Mode" : "ArkV3",                                           # 大模型名称，该参数固定取值： ArkV3
                    "EndPointId" : end_point_id,                                # 推理接入点。使用方舟大模型时必填。
                    "MaxTokens" : 1024,                                         # 非必填，输出文本的最大token数，默认 1024
                    "Temperature" : 0.1,                                        # 非必填，用于控制生成文本的随机性和创造性，值越大随机性越高。取值范围为（0,1]，默认值为 0.1
                    "TopP" : 0.3,                                               # 非必填，用于控制输出tokens的多样性，值越大输出的tokens类型越丰富。取值范围为（0,1]，默认值为 0.3
                    "SystemMessages" : [                                        # 非必填，大模型 System 角色预设指令，可用于控制模型输出。
                        system_messages
                    ],
                    "UserPrompts" : [                                          # 非必填，大模型 User 角色预设 Prompt，可用于增强模型的回复质量，模型回复时会参考此处内容。
                    ],
                    "Prefill" : llm_prefill,                                    # 非必填, 将 ASR 中间结果提前送入大模型进行处理以降低延时。开启后会产生额外模型消耗。默认值 false
                    "HistoryLength" : 3,                                        # 非必填，大模型上下文长度，默认 3。
                    # "ThinkingType": "disabled",                               # 关闭或开启大模型的深度思考能力。若你使用的是具备深度思考能力的模型，强烈建议通过该字段关闭模型的深度思考能力（disabled），以避免智能体回复耗时过长，影响对话的流畅性。参考：
                    # "Tools" : [...]                                           # 非必填，使用 Function calling 功能时，模型可以调用的工具列表 参考：https://www.volcengine.com/docs/6348/1359441
                    # "VisionConfig" : {}                                       # 视觉理解能力配置。仅在推理点选择模型为 doubao-vision-pro 和 doubao-vision-lite 时生效。该功能使用说明参看 https://www.volcengine.com/docs/6348/1408245
                    "VisionConfig" : {
                        "Enable" : vision_enable,
                        "SnapshotConfig": {
                            "StreamType": 0,                                    # 截图流类型设置为主流
                            "ImageDetail": image_detail,                        # 图片处理模式为低或高细节模式。
                            "Height": image_height,                             # 截图高度为 video_height。
                            "Interval": 1000,                                   # 相邻截图之间的间隔时间为 1000 毫秒。
                            "ImagesLimit": 1                                    # 最大发送图片数量为 1。
                        }
                    }
                },
                "SubtitleConfig" : {
                    "DisableRTSSubtitle" : disable_rts_subtitle,                # 非必填，是否关闭房间内字幕回调，默认 false
                    # "ServerMessageUrl" : "Your url",                          # 非必填，用于服务端接收字幕回调
                    # "ServerMessageSignature" : "",                            # 用于你的服务端字幕回调鉴权
                    "SubtitleMode" : 0                                          # 字幕回调时是否需要对齐音频时间戳。0 对齐，1 不对齐。默认 0
                },
                "InterruptMode" : 0                                             # 非必填，智能体对话打断模式。 0: 智能体语音可以被用户语音打断 1: 不能被用户语音打断
                # "FunctionCallingConfig" : {                                   # 服务端接收 Function calling 函数工具调用的信息指令配置。
                #     "ServerMessageUrl" : "Your URL",                          # 服务端接收 Function calling 函数工具调用的信息指令的 URL 地址。功能使用详情参看 https://www.volcengine.com/docs/6348/1359441#callingconfig 
                #     "ServerMessageSignature" : ""                             # 鉴权签名。
                # }
            },
            "AgentConfig" : {
                "TargetUserId" : [room_info["uid"]],                            # 房间内客户端 SDK 用户对应的 UserId。仅支持传入一个 UserId。注意该值是一个数组
                "WelcomeMessage" : "你好,有什么可以帮到你的吗",                   # 智能体启动后的欢迎词。
                "UserId" : room_info["bot_uid"],                                # 智能体的user id
                "EnableConversationStateCallback" : enable_conversation_state_callback, # 是否接收任务状态变化回调。默认值为 false
                "Burst" : {
                    "Enable" : enable_burst,                                    # 是否开启音频快速发送。默认值 false
                    "BufferSize" : burst_buffer_size,                           # 接收音频快速发送片段时，客户端可缓存的最大音频时长。取值范围为[10,3600000]，单位为 ms，默认值为 10
                    "Interval" : burst_interval                                 # 音频快速发送结束后，其他音频内容发送间隔。取值范围为[10,600]，单位为 ms，默认值为10
                }
            }
        }
        if fc_tools != None and len(fc_tools) > 0 :
            request_body["Config"]["LLMConfig"]["Tools"] = fc_tools

        request_body_str = json.dumps(request_body)
        canonical_query_string = "Action=%s&Version=%s" % (RTC_API_START_VOICE_CHAT_ACTION, RTC_API_VERSION)
        code, response = RtcApiRequester.request_rtc_api(RTC_API_HOST, "POST", "/", canonical_query_string, None, request_body_str, AK, SK)
        print("request_rtc_api start code:", code)
        print("request_rtc_api start response:", response)
        if code == RESPONSE_CODE_SUCCESS:
            if "Result" in response and response["Result"] == "ok":
                return None
            else:
                return response["ResponseMetadata"]["Error"]["Message"]
        else:
            if response != None:
                return response["ResponseMetadata"]["Error"]["Message"]
            else:
                return "request rtc api response code " + str(code)
        return None

###################################### stop voice chat #######################################
    def stop_voice_chat(self, json_obj):
        # 参考 https://www.volcengine.com/docs/6348/1404672
        if "room_id" not in json_obj or "task_id" not in json_obj or "app_id" not in json_obj:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "stop_voice_chat: \"room_id\", \"task_id\", \"app_id\" must be in json")
            return
        
        ret = self.request_stop_voice_chat(json_obj)
        if ret == None:
            resp_obj = {
                "data" : json_obj
            }
            self.response_data(RESPONSE_CODE_SUCCESS, "", resp_obj)
        else:
            self.response_data(RESPONSE_CODE_SERVER_ERROR, ret)
    
    def request_stop_voice_chat(self, json_obj):
        # 参考 https://www.volcengine.com/docs/6348/1404672
        request_body = {
            "AppId" : json_obj["app_id"],      # rtc app id
            "RoomId" : json_obj["room_id"],    # rtc 房间 id
            "TaskId" : json_obj["task_id"]     # rtc 客户端用户id
        }

        request_body_str = json.dumps(request_body)
        canonical_query_string = "Action=%s&Version=%s" % (RTC_API_STOP_VOICE_CHAT_ACTION, RTC_API_VERSION)
        code, response = RtcApiRequester.request_rtc_api(RTC_API_HOST, "POST", "/", canonical_query_string, None, request_body_str, AK, SK)
        print("request_rtc_api stop code:", code)
        print("request_rtc_api stop response:", response)
        if code == RESPONSE_CODE_SUCCESS:
            if "Result" in response and response["Result"] == "ok":
                return None
            else:
                return response["ResponseMetadata"]["Error"]["Message"]
        else:
            if response != None:
                return response["ResponseMetadata"]["Error"]["Message"]
            else:
                return "request rtc api response code " + str(code)
        return None

###################################### update voice chat #####################################
    def update_voice_chat(self, json_obj):
        # 更新智能体详细信息请参考 https://www.volcengine.com/docs/6348/1404671
        if "room_id" not in json_obj or "task_id" not in json_obj or "app_id" not in json_obj or "command" not in json_obj:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "update_voice_chat: \"room_id\", \"task_id\", \"app_id\", \"command\" must be in json")
            return
        update_commands = {"interrupt", "function", "external_text_to_speech", "external_prompts_for_llm", "external_text_to_llm", "finish_speech_recognition"}
        if json_obj["command"] not in update_commands:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "update_voice_chat: your command == " + json_obj["command"] + ", command must be in " + str(update_commands))
            return
        required_message_commands = {"function", "external_text_to_speech", "external_prompts_for_llm", "external_text_to_llm"}
        if json_obj["command"] in required_message_commands and "message" not in json_obj:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "update_voice_chat: your command == " + json_obj["command"] + ", \"message\" must be in json")
            return
        
        required_interrupt_mode_commands = {"external_text_to_speech", "external_text_to_llm"}
        if json_obj["command"] in required_interrupt_mode_commands and "interrupt_mode" not in json_obj:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "update_voice_chat: your command == " + json_obj["command"] + ", \"interrupt_mode\" must be in json, interrupt_mode == 1, 2, or 3")
            return
        if "interrupt_mode" in json_obj:
            if json_obj["interrupt_mode"] not in {1, 2, 3}:
                self.response_data(RESPONSE_CODE_REQUEST_ERROR, "update_voice_chat: your command == " + json_obj["command"] + ", \"interrupt_mode\" must be in json, interrupt_mode == 1, 2, or 3")
                return
        ret = self.request_update_voice_chat(json_obj)
        if ret == None:
            resp_obj = {
                "data" : json_obj
            }
            self.response_data(RESPONSE_CODE_SUCCESS, "", resp_obj)
        else:
            self.response_data(RESPONSE_CODE_SERVER_ERROR, ret)
    
    def request_update_voice_chat(self, json_obj):
        # 参考 https://www.volcengine.com/docs/6348/1404671
        update_commands_map = {
            "interrupt" : "Interrupt",
            "function" : "Function",
            "external_text_to_speech" : "ExternalTextToSpeech",
            "external_prompts_for_llm" : "ExternalPromptsForLLM",
            "external_text_to_llm" : "ExternalTextToLLM",
            "finish_speech_recognition" : "FinishSpeechRecognition"
        }
        parsed_command = update_commands_map[json_obj["command"]]
        request_body = {
            "AppId" : json_obj["app_id"],      # rtc app id
            "RoomId" : json_obj["room_id"],    # rtc 房间 id
            "TaskId" : json_obj["task_id"],    # 创建智能体时用的TaskId
            "Command" : parsed_command,        # 更新指令 interrupt： 打断智能体说话；function：传回工具调用信息指令。
            # "Message" : "..."                # 工具调用信息指令，格式为 Json 转译字符串。Command 取值为 function时，Message必填。
            # "InterruptMode" : 1              # 打断模式。取值范围为 1, 2, 3. 当 command 为 ExternalTextToSpeech 或 ExternalTextToLLM 时为该参数必填。
        }
        if "interrupt_mode" in json_obj:
            request_body["InterruptMode"] = json_obj["interrupt_mode"]
        if json_obj["command"] == "function":
            # function calling 数据， 参考 https://www.volcengine.com/docs/6348/1359441
            # 客户端传来的message数据是一个json字符串，内容如下：
            # {
            #     "subscriber_user_id" : "",
            #     "tool_calls" : 
            #     [
            #         {
            #             "function" : 
            #             {
            #                 "arguments" : "{\\"location\\": \\"\\u5317\\u4eac\\u5e02\\"}",
            #                 "name" : "get_current_weather"
            #             },
            #             "id" : "call_py400kek0e3pczrqdxgnb3lo",
            #             "type" : "function"
            #         }
            #     ]
            # }
            
            print(json_obj["message"])
            message_json_obj = parse_json(json_obj["message"])
            if message_json_obj == None:
                self.response_data(RESPONSE_CODE_REQUEST_ERROR, "Post data is not a json string.")
                return
            # 下面代码只是示例，要根据实际情况，解析函数名称和参数，做出真实的响应
            if "tool_calls" not in message_json_obj or len(message_json_obj["tool_calls"]) <= 0 or "id" not in message_json_obj["tool_calls"][0]:
                self.response_data(RESPONSE_CODE_REQUEST_ERROR, "function calling message is error.")
                return
            message_body = {
                "ToolCallID" : message_json_obj["tool_calls"][0]["id"],
                "Content" : "今天天气很好，阳光明媚，偶尔有微风。"
            }
            
            request_body["Message"] = json.dumps(message_body)
        elif "message" in json_obj:
            request_body["Message"] = json_obj["message"]
        
        request_body_str = json.dumps(request_body)
        canonical_query_string = "Action=%s&Version=%s" % (RTC_API_UPDATE_VOICE_CHAT_ACTION, RTC_API_VERSION)
        code, response = RtcApiRequester.request_rtc_api(RTC_API_HOST, "POST", "/", canonical_query_string, None, request_body_str, AK, SK)
        print("request_rtc_api update code:", code)
        print("request_rtc_api update response:", response)
        if code == RESPONSE_CODE_SUCCESS:
            if "Result" in response and response["Result"] == "ok":
                return None
            else:
                return response["ResponseMetadata"]["Error"]["Message"]
        else:
            if response != None:
                return response["ResponseMetadata"]["Error"]["Message"]
            else:
                return "request rtc api response code " + str(code)
        return None


##############################################################################################
    def response_data(self, code, msg, extra_data = None):
        self.send_response(code)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        ret_data = {
            "code": code,
            "msg" : msg
        }

        if extra_data != None:
            for k, v in extra_data.items():
                ret_data[k] = v
        self.wfile.write(json.dumps(ret_data).encode())


    def parse_post_data(self):
        # check headers
        content_type = self.headers.get("Content-Type")
        authorization = self.headers.get("Authorization")
        if content_type != "application/json":
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "header Content-Type error, must be application/json.")
            return None
        if authorization == None or authorization == "":
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "header Authorization error, Authorization not be set.")
            return None
        if authorization != ("af78e30" +  RTC_APP_ID):
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "header Authorization error, Bad Authorization.")
            return None
        
        # check post_data is json
        content_length = int(self.headers['Content-Length'])
        post_data = self.rfile.read(content_length).decode('utf-8')
        json_obj = None
        try:
            json_obj = json.loads(post_data)
        except Exception as e:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "post data is not json string.")
            return None
        return json_obj



# 启动服务
with socketserver.TCPServer(("", PORT), RtcAigcHTTPRequestHandler) as httpd:
    print("serving at port", PORT)
    httpd.serve_forever()
