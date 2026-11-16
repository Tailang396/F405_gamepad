# 冰原狼 2 USB 报文解析（可移植 C）

`direwolf2.c` / `direwolf2.h` 是不依赖操作系统、Windows API、动态分配或特定 STM32 库的 C99 解析器。它解析一个完整的 Xbox 360 兼容 USB 输入报文，不负责连接 USB 设备。`parser_test` 使用合成报文检查解析边界和字段。

Windows 默认目标 `gamepad` 通过 DirectInput 持续显示手柄实时状态。可选目标 `parser_demo` 使用 `demo.c` 每 250 毫秒解析并输出同一份模拟报文；不会读取真实设备。用 CLion 停止按钮或 Ctrl+C 结束。`demo_delay.cpp` 仅供电脑演示计时，使用 C++ 标准库；可移植的解析模块仍是纯 C。收到真实 USB 报文时，把报文缓冲区和实际长度传给 `dw2_parse_report` 即可。电脑端可选目标 `report_stdin` 从标准输入逐行接收十六进制报文，并调用相同的解析器。STM32 上把 `direwolf2.c` / `direwolf2.h` 接入 USB Host 接收回调，由串口或其他设备输出。示例的 `printf` 在 STM32 上需要配置标准输出重定向，计时使用板上定时器或调度器。

Windows 可选目标 `pc_monitor` 使用 DirectInput 输出真实设备的 Windows 状态，不调用上述 USB 报文解析器；它已在用户的 CLion 环境中读取成功，输出不是原始 USB 报文，不能移植到 STM32。该目标默认不参与构建。

Windows 目标 `usb_enum` 枚举 USB 集线器和连接设备，读取配置描述符并打印接口与端点。它只查询设备，不接收手柄输入报文，也不能移植到 STM32。通过 CLion 运行 `usb_enum` 后，可查找 `VID=045E PID=028E`；其他连接模式或设备可能有不同 ID。若集线器返回 error 5，表示访问被拒绝，描述符未能读取；可在具有相应权限的正常 Windows 会话中运行并查看错误信息。

原 Windows XInput 程序仍保存在 `main.cpp`，默认不构建。需要恢复构建时，在 CLion 的 CMake 选项中设置 `-DBUILD_XINPUT_MONITOR=ON`，构建目标 `xinput_monitor`。

## 如何接入 STM32 USB Host

1. 选择具有 USB Host/OTG Host 功能的 STM32，配置 USB Host 时钟、引脚、电源及 VBUS 供电；通过可靠的 5 V 供电电路为手柄或接收器供电。
2. 将手柄或接收器连接到 Host，完成枚举。查看实际设备描述符，确定配置、接口、备用设置、interrupt IN 端点、最大包长和轮询间隔。按实际接口类型编写或配置 Host 类驱动，并处理连接、断开和重新枚举。不要在没有验证的情况下假定 VID/PID、端点地址或初始化流程。
3. 用 USB Host 库在该输入端点接收一个完整传输。仅在接收完成后，将实际收到的字节数传给 `dw2_parse_report`，从报文的第一个字节开始；输出状态可供应用使用。接收下一包，处理超时/失败/断开。若缓冲区由 DMA 使用，请按芯片要求处理对齐、缓存一致性和缓冲区所有权。

```c
#include "direwolf2.h"

/* 在 USB Host 的接收完成回调或任务中调用；data 的格式须与本协议一致。 */
void on_usb_input(const uint8_t *data, size_t actual_length) {
    dw2_state state;
    if (dw2_parse_report(data, actual_length, &state)) {
        if (state.buttons & DW2_A) { /* A 按下 */ }
        /* 使用 state.lx、state.ly、state.rt 等；由应用决定如何输出。 */
    }
}
```

冰原狼 2 的 USB Xbox 兼容接口不能假定是标准 USB HID。通用 HID Host 库通常只处理 HID 类设备，未必能绑定 Xbox 兼容的厂商类接口；需要使用能够访问该接口的 Host 类驱动。若手柄实际处于 Switch、蓝牙或其他 HID 模式，报文可能完全不同，不能直接使用此解析器。具体硬件的枚举、类驱动、端点接收、任何必要的初始化和串口输出仍需针对芯片和库实现；本工程没有完成这些底层部分，也尚未在真实设备原始报文或 STM32 上验证。

## 格式与限制

参考第三方 [flydigi_ds4 报文解析](https://github.com/fsagugu-web/flydigi_ds4/blob/main/source/xinput.c)：报文开头为 `00`，第二字节至少 `14`；偏移 2–3 为小端按键位，4–5 为左右扳机（0–255），6–13 为四个小端有符号 16 位摇杆轴（−32768～32767）。偏移 14–16 被该项目解释为两个打包的有符号 12 位运动值，仅收到至少 17 字节时设置 `motion_available`。这些运动值属于实验性解析，不保证是原始陀螺仪、加速度、物理单位或适用于所有固件。短于 14 字节或头部不符的输入会被拒绝，且不会改变输出结构。

`motion_available` 仅表示报文长度足够，不代表运动数据有效。摇杆未加死区；背键是否作为独立位出现尚未验证。协议研究见 [飞智官方手册](https://tencent-bbs.cdn.flydigi.com/flydigi/pdf/FP2.pdf)及上述第三方代码；设备模式、连接方式和固件可能影响报文格式。
