# Copyright (2025) Beijing Volcano Engine Technology Ltd.
# SPDX-License-Identifier: MIT

set(COMPONENT_SRCS "VolcRTCDemo.c AudioPipeline.c RtcHttpUtils.c RtcBotUtils.c" )
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -Wno-unused-variable -Wfatal-errors -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-format -Wno-implicit-function-declaration -Wno-pointer-sign -Wno-double-promotion  -Wno-unused-value -Wno-switch -Wno-return-type -g -O3")  
