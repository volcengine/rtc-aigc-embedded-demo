本教程将指导你如何在乐鑫 ESP32-S3-Korvo-2 或 AtomS3R 开发板上运行火山引擎 RTC AIGC Demo，实现与 AI 实时对话。

## 快速开始

具体操作，请参考 [官网文档](https://www.volcengine.com/docs/6348/1806625)。

## 运行设备端（乐鑫）

以下操作以 macOS 操作系统为例。

### 环境与硬件要求
- 乐鑫 ESP32-S3-Korvo-2 或 AtomS3R 开发板。
- USB 数据线：两条 A 转 Micro-B 数据线，一条作为电源线，一条作为串口线。
- PC 设备服：编译和烧录。支持 Windows、Linux 或者 macOS 操作系统。（本文操作以 macOS 为例）

### 配置乐鑫环境

详见[开发环境配置文档](https://docs.espressif.com/projects/esp-idf/zh_CN/stable/esp32s3/get-started/index.html)。

1. 安装 CMake 和 Ninja 编译工具。
    ```bash
    brew install cmake ninja dfu-util
    ```
2. 将乐鑫 ADF 框架克隆到本地，并同步各子仓（submodule）代码。
   > **注意**：Demo 中使用的 ADF 版本为 `eca11f20e56f9b5321b714da4305e123672d92a9`，对应 IDF 版本为 `v5.4`，请确保 ADF 版本与 IDF 版本匹配。
    ```bash
    # 1. clone 乐鑫 ADF 框架
    git clone https://github.com/espressif/esp-adf.git
    # 2. 进入esp-adf目录
    cd esp-adf
    # 3. 切换到乐鑫 ADF 指定版本
    git reset --hard eca11f20e56f9b5321b714da4305e123672d92a9
    # 4. 同步各子仓代码
    git submodule update --init --recursive
    ```
3. 安装乐鑫 esp32s3 开发环境相关依赖。
    ```bash
    ./install.sh esp32s3
    ```
    成功安装所有依赖后，命令行会出现如下提示：
    ```bash
    All done! You can now run:
    . ./export.sh
    ```    
    > 如在上述任何步骤中遇到以下错误:
    > `<urlopen error [SSL: CERTIFICATE_VERIFY_FAILED] certificate verify failed: unable to get local issuer certificate (_ssl.c:xxx)`
    > 可前往**访达->应用程序->Python** 文件夹，点击 `Install Certificates.command` 安装证书。更多信息，请参考 [安装 ESP-IDF 工具时出现的下载错误](https://github.com/espressif/esp-idf/issues/4775)。
4. 设置环境变量。
    > **每次打开命令行窗口均需要运行该命令进行设置。**
    ```bash
    . ./export.sh
    ```
### 下载并配置工程
1. 将实时对话式 AI 硬件示例工程克隆到 乐鑫 ADF examples 目录下。
   1. 进入 esp-adf/examples 目录。
    ```bash
    cd $ADF_PATH/examples
    ```
   2. 克隆实时对话式 AI 硬件示例工程。
   ```bash
   git clone https://github.com/volcengine/rtc-aigc-embedded-demo.git
   ```
2. 禁用乐鑫工程中的火山组件。
   1. 进入 esp-adf 目录。
    ```bash
    cd $ADF_PATH
    ```
   2. 禁用乐鑫工程中的火山组件。
    ```bash
    git apply $ADF_PATH/examples/rtc-aigc-embedded-demo/0001-feat-disable-volc-esp-libs.patch
    ```
### 编译固件
进入 `esp-adf/examples/rtc-aigc-embedded-demo/client/espressif/esp32s3_demo` 目录下编译固件。
1. 进入 esp32s3_demo 目录。
    ```bash
    cd $ADF_PATH/examples/rtc-aigc-embedded-demo/client/espressif/esp32s3_demo
    ```
2. 设置编译目标平台。
    ```bash
    idf.py set-target esp32s3
    ```
3. 设置 实例ID、产品ID、产品秘钥、设备ID等参数。
    ```bash
    idf.py menuconfig
    ```
    进入 `Example Configuration` 菜单，在 `volcano instance id` 中填入你的实例ID，在 `volcano product key` 中填入你的产品ID，在 `volcano product secret` 中填入你的产品秘钥，在 `device name` 中填入你的设备ID， 在 `bot id` 中填入你的智能体ID，并保存。
4. 设置开发板型号。
    ```bash
    idf.py menuconfig
    ```
    进入 `Audio HAL` 菜单，在 `Audio board` 中选择你的开发板型号。(例如: 方舟开发板选择 `M5STACK-ATOMS3R`)，并保存。
5. 编译固件。
    ```bash
    idf.py build
    ```
### 烧录并运行示例 Demo
1. 打开乐鑫开发板电源开关。
2. 烧录固件。
    ```bash
    idf.py flash
    ```
3. 运行示例 Demo 并查看串口日志输出。
    ```bash
    idf.py monitor
    ```
4. Wi-Fi 配网。
    1. 手机找到名如 “VolcRTC-XXXXXX” 的 Wi-Fi 热点，连接上 Wi-Fi。
    2. 打开浏览器，在地址栏输入 `http://192.168.4.1`，进入 Wi-Fi 配网页面。
    3. 输入 Wi-Fi 名称和密码，点击提交。

    > **注意**：如果需更换 Wi-Fi，请重启设备。如果设备重启后无法连接到之前保存的 Wi-Fi（例如超出了范围或旧网络已关闭），请等待 30s 进入配网模式，再重新执行上面 Wi-Fi 配网的 3 个步骤。