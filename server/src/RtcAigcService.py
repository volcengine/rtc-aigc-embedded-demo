# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

import http.server
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
# START_VOICE_CHAT_URL = "https://rtc.volcengineapi.com?Action=StartVoiceChat&Version=2024-06-01"
# STOP_VOICE_CHAT_URL = "https://rtc.volcengineapi.com?Action=StopVoiceChat&Version=2024-06-01"
# UPDATE_VOICE_CHAT_URL = "https://rtc.volcengineapi.com?Action=UpdateVoiceChat&Version=2024-06-01"
RTC_API_HOST = "rtc.volcengineapi.com"
RTC_API_START_VOICE_CHAT_ACTION = "StartVoiceChat"
RTC_API_STOP_VOICE_CHAT_ACTION = "StopVoiceChat"
RTC_API_UPDATE_VOICE_CHAT_ACTION = "UpdateVoiceChat"
RTC_API_VERSION = "2024-06-01"


class RtcAigcHTTPRequestHandler(http.server.BaseHTTPRequestHandler):
    '''
    StartVoiceChat
    curl --location 'http://127.0.0.1:8080/startvoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: hehehe' \
    --data '{
        "bot_id": "ep-20240729172503-mmg9b",
        "voice_id": "zh_female_meilinvyou_moon_bigtts"
    }'


    StopVoiceChat
    curl --location 'http://127.0.0.1:8080/stopvoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: hehehe' \
    --data '{
        "app_id": "66bb6632f55d550120fb5c94",
        "room_id": "bf410694b3a34a3aa980b6e85613200d",
        "uid": "client_bf410694b3a34a3aa980b6e85613200d"
    }'


    UpdateVoiceChat
    打断智能体说话
    curl --location 'http://127.0.0.1:8080/updatevoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: hehehe' \
    --data '{
        "app_id": "66bb6632f55d550120fb5c94",
        "room_id": "bf410694b3a34a3aa980b6e85613200d",
        "uid": "client_bf410694b3a34a3aa980b6e85613200d",
        "command": "interrupt"
    }'

    处理function calling
    curl --location 'http://127.0.0.1:8080/updatevoicechat' \
    --header 'Content-Type: application/json' \
    --header 'Authorization: hehehe' \
    --data '{
        "app_id": "66bb6632f55d550120fb5c94",
        "room_id": "bf410694b3a34a3aa980b6e85613200d",
        "uid": "client_bf410694b3a34a3aa980b6e85613200d",
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
        # 根据业务情况，生成 room_id，用户id 或者 从客户端请求中获取
        # 这里简单生成一个随机的 room_id 和 user_id
        uuid_str = uuid.uuid4().hex
        room_id = "G711A" + uuid_str # 根据房间id G711A开头，音频编码格式为g711a
        user_id = "user" + uuid_str
        expire_time = int(time.time()) + 3600 * 48 # 48h
        token = AccessToken.AccessToken(RTC_APP_ID, RTC_APP_KEY, room_id, user_id)
        token.add_privilege(AccessToken.PrivSubscribeStream, expire_time)
        token.add_privilege(AccessToken.PrivPublishStream, expire_time)
        token.expire_time(expire_time)

        token_str = token.serialize()
        room_info = {
            "room_id" : room_id,
            "uid" : user_id,
            "app_id" : RTC_APP_ID,
            "token" : token_str
        }
        print(room_info)
        return room_info
    
    def request_start_voice_chat(self, room_info, json_obj):
        
        if "bot_id" in json_obj:
            bot_id = json_obj["bot_id"]
        else:
            bot_id = DEFAULT_BOT_ID
        
        if "voice_id" in json_obj:
            voice_id = json_obj["voice_id"]
        else:
            voice_id = DEFAULT_VOICE_ID
        
        # 参考 https://www.volcengine.com/docs/6348/1316243
        request_body = {
            "AppId" : room_info["app_id"],                               # RTC App id
            "RoomId" : room_info["room_id"],                             # RTC房间id
            "UserId" : room_info["uid"],                                 # RTC客户端用户id
            "config" : {
                #  "BotName" : "",                                       # 非必填，RTC智能体用户id 
                "IntterruptMode" : 0,                                    # 非必填，智能体对话打断模式。 0: 智能体语音可以被用户语音打断 1: 不能被用户语音打断
                "ASRConfig" : {
                    "AppId" : ASR_APP_ID,                                # ASR App ID
                    "Cluster" : "volcengine_streaming_common",           # ASR Cluster ID, 默认是通用的 cluster id "volcengine_streaming_common"
                },
                "TTSConfig" : {
                    "IgnoreBracketText" : [1, 2, 3, 4, 5],               # 非必填， 过滤大模型生成的文本中符号 1:"（）" 2:"()", 3:"【】", 4:"[]", 5:"{}".默认不过滤
                    "Provider" : "volcano",                              # TTS 服务供应商
                    "ProviderParams" : {
                        "app" : {
                            "appid" : TTS_APP_ID,                        # TTS App ID
                            "cluster" : "volcano_tts"                    # 非必填， TTS Cluster ID. default "volcano_tts"
                        },
                        "audio" : {
                            "voice_type" : voice_id,                     # 非必填，音色类型
                            "speed_ratio" : 1.0,                         # 非必填，语速，默认 1.0
                            "volume_ratio" : 1.0,                        # 非必填，音量，默认 1.0
                            "pitch_ratio" : 1.0,                         # 非必填，音调，默认 1.0
                        }
                    }
                },
                "LLMConfig" : {
                    "Mode": "ArkV3",                                     # 模型类型
                    "EndPointId": bot_id,                                # 推理接入点。使用方舟大模型时必填。
                    "MaxTokens": 1024,                                   # 非必填，输出文本的最大token数，默认 1024
                    "Temperature": 0.1,                                  # 非必填，用于控制生成文本的随机性和创造性，值越大随机性越高。取值范围为（0,1]，默认值为 0.1
                    "TopP": 0.3,                                         # 非必填，用于控制输出tokens的多样性，值越大输出的tokens类型越丰富。取值范围为（0,1]，默认值为 0.3
                    "SystemMessages": [                                  # 非必填，大模型 System 角色预设指令，可用于控制模型输出。
                        "你是小宁，性格幽默又善解人意。你在表达时需简明扼要，有自己的观点。"
                    ],
                    "UserMessages": [                                    # 非必填，大模型 User 角色预设 Prompt，可用于增强模型的回复质量，模型回复时会参考此处内容。
                        "user:\"你是谁\"",
                        "assistant:\"我是问答助手\"",
                        "user:\"你能干什么\"",
                        "user:\"我能回答问题\""
                    ],
                    "HistoryLength": 3,                                  # 非必填，大模型上下文长度，默认 3。
                    "WelcomeSpeech": "你好有什么可以帮到你的吗",            # 非必填，智能体启动后的欢迎词。
                },
                "SubtitleConfig" : {
                    "DisableRTSSubtitle" : True,                        # 非必填，是否关闭房间内字幕回调，默认 false
                },
            },
        }

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
        if "room_id" not in json_obj or "uid" not in json_obj or "app_id" not in json_obj:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "stop_voice_chat: \"room_id\", \"uid\", \"app_id\" must be in json")
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
        # 参考 https://www.volcengine.com/docs/6348/1316244
        request_body = {
            "AppId" : json_obj["app_id"],      # rtc app id
            "RoomId" : json_obj["room_id"],    # rtc 房间 id
            "UserId" : json_obj["uid"]         # rtc 客户端用户id
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
        if "room_id" not in json_obj or "uid" not in json_obj or "app_id" not in json_obj or "command" not in json_obj:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "update_voice_chat: \"room_id\", \"uid\", \"app_id\", \"command\" must be in json")
            return
        
        if json_obj["command"] == "function" and "message" not in json_obj:
            self.response_data(RESPONSE_CODE_REQUEST_ERROR, "update_voice_chat: your command == function, \"message\" must be in json")
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
        # 参考 https://www.volcengine.com/docs/6348/1316245
        request_body = {
            "AppId" : json_obj["app_id"],      # rtc app id
            "RoomId" : json_obj["room_id"],    # rtc 房间 id
            "UserId" : json_obj["uid"],        # rtc 客户端用户id
            "Command" : json_obj["command"]    # 更新指令 interrupt： 打断智能体说话；function：传回工具调用信息指令。
            # "Message" : "..."                # 工具调用信息指令，格式为 Json 转译字符串。Command 取值为 function时，Message必填。
        }
        if json_obj["command"] == "function":
            # function calling 数据， 参考 https://www.volcengine.com/docs/6348/1359441
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
            try:
                json_obj = json.loads(json_obj["message"])
            except Exception as e:
                self.response_data(RESPONSE_CODE_REQUEST_ERROR, "post data is not json string.")
                return
            # 下面代码只是示例，要根据实际情况，解析函数名称和参数，做出真实的响应
            message_body = {
                "ToolCallID" : json_obj["tool_calls"][0]["id"],
                "Content" : "今天天气很好，阳光明媚，偶尔有微风。"
            }
            request_body["Message"] = json.dumps(message_body)
        
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
