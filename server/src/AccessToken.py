# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

# token生成代码来自 https://www.volcengine.com/docs/6348/70121
# 其它语言可以通过上面的链接获取
import base64
import hmac
import random
import struct
import time
from hashlib import sha256
from collections import OrderedDict

VERSION = "001"
VERSION_LENGTH = 3

APP_ID_LENGTH = 24

PrivPublishStream = 0

# not exported, do not use directly
privPublishAudioStream = 1
privPublishVideoStream = 2
privPublishDataStream = 3

PrivSubscribeStream = 4

class AccessToken:
    # Initializes token struct by required parameters.
    def __init__(self, app_id, app_key, room_id, user_id):
        random.seed(time.time())
        self.app_id = app_id
        self.app_key = app_key
        self.room_id = room_id
        self.user_id = user_id
        self.issued_at = int(time.time())
        self.nonce = random.randint(1, 99999999)
        self.expire_at = 0
        self.privileges = {}

    # AddPrivilege adds permission for token with an expiration.
    def add_privilege(self, privilege, expire_ts):
        if self.privileges is None:
            self.privileges = {}

        self.privileges[privilege] = expire_ts
        if privilege == PrivPublishStream:
            self.privileges[privPublishVideoStream] = expire_ts
            self.privileges[privPublishAudioStream] = expire_ts
            self.privileges[privPublishDataStream] = expire_ts

    # ExpireTime sets token expire time, won't expire by default.
    # The token will be invalid after expireTime no matter what privilege's expireTime is.
    def expire_time(self, expire_ts):
        self.expire_at = expire_ts

    def pack_msg(self):
        m = pack_uint32(self.nonce)
        m += pack_uint32(self.issued_at)
        m += pack_uint32(self.expire_at)
        m += pack_string(self.room_id)
        m += pack_string(self.user_id)
        m += pack_map_uint32(self.privileges)
        return m

    # Serialize generates the token string
    def serialize(self):
        m = self.pack_msg()
        signature = hmac.new(self.app_key.encode('utf-8'), m, sha256).digest()
        content = pack_bytes(m) + pack_bytes(signature)

        return VERSION + self.app_id + base64.b64encode(content).decode('utf-8')

    # Verify checks if this token valid, called by server side.
    def verify(self, key):
        if 0 < self.expire_at < int(time.time()):
            return False

        self.app_key = key
        return hmac.new(self.app_key.encode('utf-8'), self.pack_msg(), sha256).digest() == self.signature

# Parse retrieves token information from raw string
def parse(raw):
    try:
        if len(raw) <= VERSION_LENGTH:
            return
        if raw[:VERSION_LENGTH] != VERSION:
            return

        token = AccessToken("", "", "", "")
        token.app_id = raw[VERSION_LENGTH:VERSION_LENGTH + APP_ID_LENGTH]

        content_buf = base64.b64decode(raw[VERSION_LENGTH + APP_ID_LENGTH:])
        readbuf = ReadByteBuffer(content_buf)

        msg = readbuf.unpack_bytes()
        token.signature = readbuf.unpack_bytes()

        msgbuf = ReadByteBuffer(msg)
        token.nonce = msgbuf.unpack_uint32()
        token.issued_at = msgbuf.unpack_uint32()
        token.expire_at = msgbuf.unpack_uint32()
        token.room_id = msgbuf.unpack_string()
        token.user_id = msgbuf.unpack_string()
        token.privileges = msgbuf.unpack_map_uint32()
        return token

    except Exception as e:
        print("parse error:", str(e))
        return


def pack_uint16(x):
    return struct.pack('<H', int(x))


def pack_uint32(x):
    return struct.pack('<I', int(x))


def pack_int32(x):
    return struct.pack('<i', int(x))


def pack_string(string):
    return pack_bytes(string.encode('utf-8'))


def pack_bytes(b):
    return pack_uint16(len(b)) + b


def pack_map_uint32(m):
    m = OrderedDict(sorted(m.items(), key=lambda x: int(x[0])))

    ret = pack_uint16(len(m.items()))

    for k, v in m.items():
        ret += pack_uint16(k) + pack_uint32(v)
    return ret


class ReadByteBuffer:

    def __init__(self, bytes):
        self.buffer = bytes
        self.position = 0

    def unpack_uint16(self):
        len = struct.calcsize('H')
        buff = self.buffer[self.position: self.position + len]
        ret = struct.unpack('<H', buff)[0]
        self.position += len
        return ret

    def unpack_uint32(self):
        len = struct.calcsize('I')
        buff = self.buffer[self.position: self.position + len]
        ret = struct.unpack('<I', buff)[0]
        self.position += len
        return ret

    def unpack_string(self):
        return self.unpack_bytes().decode('utf-8')

    def unpack_bytes(self):
        strlen = self.unpack_uint16()
        buff = self.buffer[self.position: self.position + strlen]
        ret = struct.unpack('<' + str(strlen) + 's', buff)[0]
        self.position += strlen
        return ret

    def unpack_map_uint32(self):
        messages = {}
        maplen = self.unpack_uint16()

        for index in range(maplen):
            key = self.unpack_uint16()
            value = self.unpack_uint32()
            messages[key] = value
        return messages
