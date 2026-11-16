# 项目交接：STM32 读取飞智冰原狼 2 手柄

更新时间：2026-11-16。此文档供新对话继续项目使用；应先检查实际工程，不要假设历史版本仍是当前版本。

## 目标与用户偏好

让 STM32 作为 USB Host 直接读取飞智冰原狼 2 手柄或接收器的真实输入报文，解析按键、双摇杆和扳机，通过串口等方式输出；后续可用于其他控制。用户希望尽量把功能写入独立 `.c/.h` 文件，在 `main` 或合适的任务中调用。解析器本身不应依赖 Windows 接口。不要把固定模拟数据当成真实手柄数据。

用户没有最终确认当前手柄是通过 USB 数据线还是 2.4G 接收器与 PC 连接；实际连接方式、固件和模式可能影响描述符和报文，应再次核实。

## 当前工程位置与重要变化

- STM32 实际工程：`C:\Users\asus\Desktop\F405_gamepad`；CubeMX 配置为 `F405_gamepad.ioc`，芯片 `STM32F405RGT6`，工程使用 CMake、STM32Cube F4。
- 当前工作区 PC 工程：`D:\C_Project\Clion_project\gamepad`。
- 先前制作过 STM32 示例副本：`D:\C_Project\Clion_project\gamepad\stm32_work`。其中有先前编译通过的裸机版 `Core/Src/usbh_direwolf.c`、`gamepad_app.c`、`direwolf2.c` 等。该副本未经硬件验证。
- **2026-11-16 重新检查：桌面实际工程 `Core/Src` 中先前的 `usbh_direwolf.c`、`gamepad_app.c`、`direwolf2.c` 已不存在。** 原因不明，不应覆盖或还原用户现有改动；先分析当前文件，再决定如何集成。
- 当前桌面工程已使用 FreeRTOS：`Core/Src/main.c` 初始化并启动内核；`Core/Src/freertos.c` 中调用 `MX_USB_HOST_Init()`；`USB_HOST/Target/usbh_conf.h` 中 `USBH_USE_OS` 为 `1U`。但 `.ioc` 中 `USBH_USE_OS-HID_FS=0`，两者存在需检查的差异，不能仅凭其中一处判断配置意图。
- 当前 `.ioc` 设置 HSE 为 **8 MHz**，PLLQ 输出 **48 MHz**；先前版本曾设为 16 MHz，现已不是当前配置。需确认开发板实际外部晶振为 8 MHz，并以当前生成代码为准。
- 当前 `.ioc` 有 `PA11=USB_OTG_FS_DM`、`PA12=USB_OTG_FS_DP`、`PA9=USART1_TX`、`PA10=USART1_RX`。当前工程有 DMA、TIM7 和 FreeRTOS；修改前查明其用户用途及代码状态。
- 本会话时可写工作区不包括桌面工程；如权限策略未变化，直接修改桌面工程需要相应授权。可以先在工作区副本实现并验证，再申请写回。不可无视用户新的工程修改。

## 真实设备：电脑枚举结果

用户在电脑运行工作区 `usb_enum` 后抓到了以下设备；之前 DirectInput 识别为 `Controller (Flydigi Direwolf)`。同一 VID/PID 也可能由其他 Xbox 兼容设备使用，设备身份依据是用户描述和 Windows 设备名，而非 VID/PID 唯一证明。

```text
VID=045E PID=028E deviceClass=FF subclass=FF protocol=FF
CONFIG value=1 interfaces=1 total=48 attributes=0x80 maxPower=500 mA
INTERFACE number=0 alt=0 endpoints=2 class=FF subclass=5D protocol=01
descriptor type=0x21 length=16
ENDPOINT address=0x81 direction=IN type=interrupt maxPacket=64 interval=2
ENDPOINT address=0x02 direction=OUT type=interrupt maxPacket=64 interval=4
```

关键结论：当前模式的接口类别 `FF/5D/01` 是厂商自定义接口，不是标准 HID 或 CDC。仅在 CubeMX 选择 HID/虚拟串口无法自动读取它，需要 USB Host 自定义类处理。不要将描述符的 `0x21` 自动视为标准 HID 描述符。

描述符声明最大总线电流 500 mA，并非已测实际电流。STM32 Host 需要有合适的 5 V VBUS 供电、电源开关/保护，检查用户实际电路。当前桌面工程 `USB_HOST/Target/usbh_conf.c` 的 VBUS 驱动位置仍是 `ToDo` 占位，没有实际控制 GPIO；不要假定插入后已有供电。PA9/PA10 与 USB 的 VBUS/ID 相关功能有复用关系，要结合硬件及配置检查。

## 数据解析及验证边界

工作区 [direwolf2.c](direwolf2.c) 和 [direwolf2.h](direwolf2.h) 是纯 C 报文解析器；`dw2_parse_report(data, length, &state)` 使用本次实际接收长度，解析失败时不改输出。现有的猜测格式参考第三方开源项目，不是针对用户设备的实测结果：

| 偏移 | 第三方项目解释 |
| --- | --- |
| 0 | 报文类型 `00` |
| 1 | 参考实现要求值至少 `14`（十六进制） |
| 2–3 | 小端按键掩码 |
| 4、5 | 左右扳机，各 8 位 |
| 6–13 | 四个小端有符号 16 位摇杆轴 |
| 14–16 | 两个打包 12 位运动值，实验性，不可称已校准的原始陀螺仪 |

来源：[第三方飞智冰原狼 2 USB 报文解析](https://github.com/fsagugu-web/flydigi_ds4/blob/main/source/xinput.c)。`motion_available` 只是长度足够，不能证明运动数据有效；应先打印实际报文并逐项验证。标准 Windows XInput/DirectInput API 的状态不等于 USB 端点原始报文，独立背键和体感是否可用仍未知。

电脑程序：工作区 `gamepad` / `pc_monitor` 使用 Windows DirectInput；用户证实摇杆数值可以实时变化。`usb_enum` 用 Windows API 读取 USB 描述符，用户已成功运行。`parser_demo` 使用固定模拟报文；`report_stdin` 可解析输入的十六进制报文。PC 上此前未发现 USBPcap/Wireshark，因此尚未抓到真实手柄输入字节；USBPcap 可用于捕获 USB 请求数据，但需要安装相应工具和驱动，实际环境需重新检查。不同连接模式、固件、初始化需求均未确认。

## 下一步建议

1. 先阅读实际桌面工程、项目本地 `AGENTS.md`（若有）、当前 `.ioc`、FreeRTOS 任务、USB Host 初始化和 CMake，确认现有用户代码、USB OS 模式及两者不一致的原因；不要直接用旧的裸机副本覆盖。
2. 根据当前 FreeRTOS 与 ST USB Host 库版本，设计和注册匹配 `FF/5D/01` 的自定义类；根据枚举实际发现的 interrupt IN 端点打开 pipe、按请求完成状态取实际长度，并处理断开、重连、错误及周期轮询。不能从端点描述符推断必然不需要初始化命令；先做无写命令的验证。
3. 保持 Host 回调短小，通过适当的队列/缓冲传递报文或状态到串口任务，避免多任务竞争和阻塞 Host；对 UART/DMA、线程上下文、ISR、缓冲区生存期、抢占等按当前工程实际情况处理。
4. 先通过 USART1 输出连接状态和实际收到的原始十六进制数据，识别非输入报文；按用户按 A/B/摇杆的动作对比，然后调用纯 C 解析器，验证按键和轴。注意串口打印频率，避免过量输出影响采样。
5. 交叉编译并报告测试结果、实板验证与否；通过真实硬件记录排查 VBUS、枚举、Host 初始化与端点接收。仅凭编译通过不能声称硬件读取已完成。

## 给新对话的启动语

> 请先阅读 `D:\C_Project\Clion_project\gamepad\项目交接_冰原狼2_USB_Host.md`，检查我桌面上的 `F405_gamepad` 当前工程，再继续实现 STM32 直接读取冰原狼 2 手柄的 USB Host 代码。保留我现有工程配置，尽量用独立 `.c/.h` 文件组织，并区分已编译与已实板验证。
