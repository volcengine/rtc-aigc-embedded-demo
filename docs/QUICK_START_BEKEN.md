本教程将指导你，如何在博通 BK7258 开发板上部署和运行火山引擎 RTC AIGC Demo，实现与 AI 对话。

## 开通服务并运行服务端

具体操作，请参考 [接入全流程指引](../README.md)。

## 运行设备端（博通）

#### 硬件与环境要求
- 博通 BK7258 开发板及 Type-C 线（用于烧录和查看日志）。
- Linux PC 设备：用于编译固件，要求如下：
   - 操作系统：Ubuntu 20.04 LTS 版本及以上、Centos 7 版本及以上、Archlinux 或者 Debian 11 版本及以上。
   - 开发环境：Python 3.8 及以上版本。
- Windows PC 设备：用于烧录固件。
- 串口调试工具（如 Windows 上的 MobaXterm、PuTTY）。

### 下载 SDK（在 Linux 环境操作）

```bash
git clone --recurse-submodules https://github.com/bekencorp/bk_aidk.git -b ai_release/v2.0.1
```

### 搭建编译环境（在 Linux 环境操作)

下文以 Ubuntu 20.04 LTS 版本为例。

```bash
cd bk_aidk/
sudo bash bk_avdk/bk_idk/tools/env_tools/setup/armino_env_setup.sh
```

### 修改火山示例工程代码（在 Linux 环境操作）

1. 修改火山 RTC 工程配置文件。
   为了让设备开机后能自动连接我们的服务端，修改 `/bk_ai/project/volc_rtc/config/bk7258/config` 文件，将以下两个配置设置为 y：
    ```c
    #@ Enable start agent on device
    CONFIG_BK_DEV_STARTUP_AGENT=y
    #@ Enable start agent via volc RTC AIGC server demo
    CONFIG_VOLC_HTTP_STARTUP_AGENT=y
    ```

2. 修改火山连接配置文件。
   修改 `/bk_aidk/project/common_components/network_transfer/volc_rtc/volc_config.h` 文件，填入你的应用信息和服务器地址：
    ```c
    // RTC AppID（需与服务端配置一致）
    #define DEFAULT_RTC_APP_ID    "67582a*****4410bd1"
    // 服务端的地址和 IP，格式: "IP地址:端口号"
    #define DEFAULT_SERVER_HOST   "11***.216:8901"
    // 火山方舟大模型的 EndPointId
    #define DEFAULT_END_POINT_ID  "ep-202***60517-hlnzt"
    // 音色 ID（Voice_type）
    #define DEFAULT_VOICE_TYPE    "BV007_streaming"
    ```

3. 开启视觉理解能力（纯语音交互可忽略此步骤)。
   如果你使用的模型具备视觉理解能力，并希望启用此功能，请执行以下步骤。否则，请忽略。
   > 开启前，请确保步骤 2 中火山方舟大模型为视觉理解模型。
   
   将 `rtc-aigc-embedded-demo/client/beken/0001-enable-vlm.patch` 拷贝到 `bk_aidk` 目录下，并执行以下命令：
    ```bash
    git apply 0001-enable-vlm.patch
    ```

### 编译固件（在 Linux 环境操作）

在`bk_aidk` 目录下执行如下命令：
```bash
make bk7258 PROJECT=volc_rtc
```
编译成功后，将在 bk_aidk/build/volc_rtc/bk7258/ 目录下生成固件 `all-app.bin`。

### 烧录固件（在 Windows 环境操作）

1. **硬件连接**: 使用串口将开发板连接至 Windows 电脑，并接通电源杜邦线。
    > 更多硬件外设说明，请参考[官方文档](https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/projects/volc_rtc/index.html)。
    ![](https://portal.volccdn.com/obj/volcfe/cloud-universal-doc/upload_3ed1fbb571316166a941d04fd717dbd8.jpg)
2. **获取烧录工具**: 从 [博通官网](https://dl.bekencorp.com/tools/flash/) 下载并解压烧录工具 `BEKEN_BKFIL_V2.1.12.1_20250424.zip`。
3. **烧录**: 将在 Linux 环境下编译好的固件 `all-app.bin` 文件拷贝到 Windows 电脑。打开烧录工具，选择正确的串口号，加载固件进行烧录。
    > 如果烧录进度无响应，可以尝试短按开发板上的 `RESET` 按键。
    
    ![alt](https://portal.volccdn.com/obj/volcfe/cloud-universal-doc/upload_9735feb4e3b1ba8ee1341367e18f673e.png)
    
### 调试与运行

1. APP 配网：参考[官方文档](https://docs.bekencorp.com/arminodoc/bk_aidk/bk7258/zh_CN/v2.0.1/projects/volc_rtc/index.html#id20)进行配网操作。

2. 与 AI 对话
- 开始对话（唤醒）：对开发板麦克风说出唤醒词 `hi armino` 或 `嗨阿米诺`，设备唤醒后会播放提示音 `啊哈`，即可以进行 AI 对话。
- 停止对话（休眠）：对开发板麦克风说出关键词 `byebye armino` 或 `拜拜阿米诺`，设备检测到后会播放提示音 `byebye`，并进入睡眠，停止与AI的对话。

3. 查看日志（可选）
    1. 在 Windows 电脑上安装串口调试工具（如“串口调试助手”，可从微软商店下载）。
    2. 选择正确的 COM 端口，将波特率设置为 **115200**，打开串口即可查看设备运行日志。
    > 端口号可通过右键"我的电脑->管理->设备管理器->端口"查看，一般端口名为 `USB-SERIAL xxx(COMxx)`。
    
    ![alt](https://portal.volccdn.com/obj/volcfe/cloud-universal-doc/upload_7c18a71324127bd016889bd020568ece.png)