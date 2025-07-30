# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

set(COMPONENT_SRCS "VolcRTCDemo.c AudioPipeline.c RtcHttpUtils.c configuration_ap.c network.c" )
if (CONFIG_VOLC_RTC_MODE)
    set(COMPONENT_SRCS ${COMPONENT_SRCS} RtcBotUtils.c)
endif()
if (CONFIG_COZE_RTC_MODE)
    set(COMPONENT_SRCS ${COMPONENT_SRCS} CozeBotUtils.c)
endif()
set(COMPONENT_ADD_INCLUDEDIRS .)
set(COMPONENT_EMBED_TXTFILES assets/wifi_configuration_ap.html)

register_component()
