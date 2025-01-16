# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

import datetime
import hashlib
import hmac
import requests

def hash_sha256(content):
    return hashlib.sha256(content.encode("utf-8")).hexdigest()

def hmac_sha256(key, content):
    return hmac.new(key, content.encode("utf-8"), hashlib.sha256).digest()

def request_rtc_api(http_host, http_request_method, canonical_uri, canonical_query_string, http_headers, http_body, AK, SK):
    now = datetime.datetime.utcnow()

    # 步骤1：创建规范请求
    x_content_sha256 = hash_sha256(http_body)
    x_date = now.strftime("%Y%m%dT%H%M%SZ")
    content_type = "application/json"
    signed_headers_vec = (
        ("content-type", content_type), 
        ("host", http_host), 
        ("x-content-sha256", x_content_sha256), 
        ("x-date", x_date)
    )
    canonical_headers = "\n".join((":".join(x) for x in signed_headers_vec)) + "\n"
    signed_headers = ";".join((x[0] for x in signed_headers_vec))
    canonical_request = http_request_method + "\n" + canonical_uri + "\n" + canonical_query_string + "\n" + canonical_headers + "\n" + signed_headers + "\n" + x_content_sha256
    
    # 步骤2：创建待签字符串
    credential_scope = x_date[0:8] + "/cn-north-1/rtc/request"
    string_to_sign = "HMAC-SHA256" + "\n" + x_date + "\n" + credential_scope + "\n" + hash_sha256(canonical_request)

    # 步骤3：构建签名
    hmac_contents = credential_scope.split("/")
    hmac_contents.append(string_to_sign)
    
    signature = SK.encode("utf-8")
    for hmac_content in hmac_contents:
        signature = hmac_sha256(signature, hmac_content)
    signature = signature.hex()
    
    # 步骤4：生成Authorization
    authorization = "HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s" % (AK, credential_scope, signed_headers, signature)

    # 步骤5：发起http请求
    if canonical_uri == "/":
        canonical_uri = ""
    url = 'https://' + http_host + canonical_uri + "?" + canonical_query_string
    headers = {
        "Content-Type" : content_type, 
        "Host" : http_host, 
        "X-Content-Sha256": x_content_sha256, 
        "X-Date": x_date,
        "Authorization" : authorization
    }

    if http_headers != None:
        headers.update(http_headers)
    
    if http_request_method == "POST":
        response = requests.post(url, headers=headers, data=http_body)
    else:
        response = requests.get(url, headers=headers)
    
    return (response.status_code, response.json())
