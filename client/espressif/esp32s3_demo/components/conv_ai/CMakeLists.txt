set(SRCS "./src/volc_conv_ai.c"
          "./src/base/volc_device_manager.c"
          "./src/node/volc_ws.c"
          "./src/platform/volc_platform.c"
          "./src/util/volc_auth.c"
          "./src/util/volc_base64.c"
          "./src/util/volc_http.c"
          "./src/util/volc_json.c"
          "./third_party/mbedtls_port/tls_certificate.c"
          "./third_party/mbedtls_port/tls_client.c"
          "./third_party/webclient/src/webclient.c"
          "./third_party/websocket/websocket.c")
set(INCS "./" "./inc" "./src" "./third_party/webclient/inc" "./third_party/websocket" "./third_party/mbedtls_port")

set(COMPONENT_SRCS ${SRCS})
set(COMPONENT_ADD_INCLUDEDIRS ${INCS})
set(COMPONENT_REQUIRES mbedtls json esp_netif esp_websocket_client lwip)

if(CONFIG_VOLC_WS_MODE)
    add_definitions(-DENABLE_WS_MODE)
endif()

register_component()
