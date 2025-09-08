# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

set(COMPONENT_SRCS "VolcRTCDemo.c AudioPipeline.c configuration_ap.c network.c" )
if (CONFIG_VOLC_WS_MODE)
    add_definitions(-DENABLE_WS_MODE)
endif()

set(COMPONENT_ADD_INCLUDEDIRS .)
set(COMPONENT_EMBED_TXTFILES assets/wifi_configuration_ap.html)

register_component()
