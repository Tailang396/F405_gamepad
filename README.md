# STM32F405 USB 手柄读取

使用 STM32F405 作为 USB Host，通过数据线读取飞智冰原狼 2 手柄的原始输入报文，并解析按键、双摇杆和扳机。工程基于 STM32Cube F4、FreeRTOS 和 CMake，解析结果可通过 USART1 输出或在其他任务中使用。

## 当前功能

- 通过 USB Host 自定义类接收手柄中断 IN 报文。
- 提供原始数据快照和解析后数据两个接口。
- 解析按键位掩码、左右扳机及四个摇杆轴。
- 通过中断 OUT 端点发送原始命令，并提供震动命令封装。
- 在手柄应用任务中通过串口打印解析结果。
- 实机曾读取到 32 字节输入报文；手柄连接和断开也已在调整板卡硬件后测试。

目前只保存**最近一包**，不会缓存所有输入事件；`valid` 不包含收包超时判断。各按键、摇杆方向和实验性运动字段仍需结合实际手柄逐项验证。

## 硬件与连接

| 项目 | 当前配置 |
| --- | --- |
| 主控 | STM32F405RGT6 |
| USB | OTG FS Host，PA11 为 D−，PA12 为 D+ |
| 调试串口 | USART1，PA9 为 TX，PA10 为 RX，波特率 2000000 |
| 手柄 | 飞智冰原狼 2，通过 USB 数据线连接 |
| 调试器 | 项目调试时使用 DAPLink / OpenOCD |

实机枚举得到的设备 VID/PID 为 `045E:028E`，接口为 `FF/5D/01`，中断 IN 端点为 `0x81`，中断 OUT 端点为 `0x02`。这是厂商自定义接口，当前驱动从描述符查找端点；这些标识不保证能唯一识别设备，不同模式或固件的描述符也可能不同。

**板卡注意事项：**本项目所用开发板的原理图中，R4 将 USB D+ 上拉到 3.3 V，导致 Host 在手柄拔掉后仍检测到连接。移除该板上的 R4 后，用户反馈连接问题恢复正常。其他板卡应先核对实物和原理图，不要直接照搬此硬件修改。USB Host 还需要合适的 5 V VBUS 供电；当前工程的 VBUS 控制函数没有实际 GPIO 控制实现。

USB D+、D− 数据线上不要额外接会干扰设备插拔检测的固定上拉或下拉电阻；尤其要检查开发板是否为 USB Device 模式在 D+ 上安装了固定上拉电阻。USB Host/设备按规范使用的内部终端电阻仍然需要，不应一概去除；数据线上的串联电阻也不属于这里所说的上下拉电阻。

## 使用接口

在 FreeRTOS **任务上下文**获取解析后的手柄状态：

```c
#include "bsp_gamepad.h"

bsp_gamepad_data_t pad = bsp_gamepad_get_data();

if (pad.connected && pad.valid) {
    if ((pad.buttons & BSP_GAMEPAD_A) != 0U) {
        // A 键按下
    }
    // pad.lt、pad.rt：扳机
    // pad.lx、pad.ly、pad.rx、pad.ry：摇杆轴
}
```

也可以获取最近一包原始数据：

```c
#include "bsp_gamepad.h"

gamepad_raw_data_t raw;
bsp_gamepad_get_raw_data(&raw);

if (raw.connected && raw.length > 0U) {
    // raw.data 的前 raw.length 个字节有效
}
```

`bsp_gamepad_get_data()` 和 `bsp_gamepad_get_raw_data()` 都是读取数据快照，不直接发起 USB 传输。USB 接收由 USB Host 任务负责；SOF 中断负责通知该任务。详细字段、文件职责和数据流程见 [手柄模块说明](APP/app_gamepad/手柄模块说明.md)。

### 震动控制

在普通 FreeRTOS 任务中调用 `bsp_gamepad_set_vibration()`，选择左侧、右侧或两侧，强度为 `0～255`：

```c
#include "bsp_gamepad.h"

if (bsp_gamepad_set_vibration(BSP_GAMEPAD_RUMBLE_BOTH, 128)) {
    // 请求已接受；之后检查 bsp_gamepad_get_send_status() 的结果
}

// 上一包发送结束后，需要停止时调用：
// bsp_gamepad_set_vibration(BSP_GAMEPAD_RUMBLE_BOTH, 0);
```

每次命令会将未选中的通道设为 0；强度为 0 时关闭两侧。**当前代码返回 1 只表示请求已接受，并非 USB 发送成功**；返回 0 表示未接受（例如未连接、发送忙或参数无效）。实际传输由 USB Host 任务异步完成，可调用 `bsp_gamepad_get_send_status()` 查询 `GAMEPAD_TX_PENDING`、`GAMEPAD_TX_SENT`、`GAMEPAD_TX_FAILED` 或 `GAMEPAD_TX_DISCONNECTED`。一次只接受一包未完成的请求；`GAMEPAD_TX_SENT` 也不保证手柄执行了命令。

如果需要发送其他原始命令，可以使用 `bsp_gamepad_send_data(data, length)`。该接口会复制数据，返回 1 表示请求已接受；发送结果同样使用 `bsp_gamepad_get_send_status()` 查询。这两个接口应在 FreeRTOS 任务上下文中调用。

震动命令采用 Xbox 360 兼容格式；当前没有记录在该手柄上完成震动实测，通道对应关系和实际效果需要验证。当前示例任务每次循环会提交一次强度为 0 的停止命令，不会自动产生周期震动。

## 工程结构

```text
APP/app_gamepad/          手柄应用任务及模块说明
BSP/gamepad/              解析结果、震动封装和对外接口
BSP/gamepad/gamepad/      USB 自定义类、原始报文存储与异步发送
USB_HOST/                 STM32Cube USB Host 配置
Core/                     STM32 初始化与 FreeRTOS 任务配置
Drivers/、Middlewares/    STM32 HAL、USB Host 和 FreeRTOS 代码
F405_gamepad.ioc          STM32CubeMX 配置
```

## 构建

需要 CMake、Ninja 和 ARM GNU Toolchain（`arm-none-eabi-gcc` 等工具需在 PATH 中）。在项目根目录执行：

```sh
cmake --preset Debug
cmake --build --preset Debug
```

生成的固件为 `build/Debug/F405_gamepad.elf`，可使用 DAPLink / OpenOCD 或其他兼容调试器烧录。应用任务当前每约 10 ms 输出一次数据。

当前生成代码中 `USBH_USE_OS` 为 `1`，但 `.ioc` 中记录的 `USBH_USE_OS-HID_FS` 为 `0`。在 CubeMX 中重新生成之前，请核对并同步该设置，并确认 `USB_HOST/App/usb_host.c` 用户代码区中的自定义类注册仍然保留。

## 说明

报文解析参考 Xbox 兼容输入格式；`motion_h`、`motion_v` 是实验性字段，不能视为已经校准的体感数据。当前工程适合继续验证手柄输入；若用于持续控制设备，还需要结合应用需求处理数据超时、按键事件和失联后的行为。
