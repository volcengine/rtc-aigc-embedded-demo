# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

set(COMPONENT_SRCS "VolcRTCDemo.c AudioPipeline.c RtcHttpUtils.c RtcBotUtils.c configuration_ap.c network.c" )
set(COMPONENT_ADD_INCLUDEDIRS .)
set(COMPONENT_EMBED_TXTFILES assets/wifi_configuration_ap.html)

register_component()
