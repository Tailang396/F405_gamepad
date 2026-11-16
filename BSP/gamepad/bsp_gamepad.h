#ifndef BSP_GAMEPAD_H
#define BSP_GAMEPAD_H

#include "gamepad.h"

/* 按键位掩码；可以用 (buttons & BSP_GAMEPAD_A) 判断按键状态。 */
enum {
    BSP_GAMEPAD_UP = 0x0001, BSP_GAMEPAD_DOWN = 0x0002,
    BSP_GAMEPAD_LEFT = 0x0004, BSP_GAMEPAD_RIGHT = 0x0008,
    BSP_GAMEPAD_START = 0x0010, BSP_GAMEPAD_BACK = 0x0020,
    BSP_GAMEPAD_L3 = 0x0040, BSP_GAMEPAD_R3 = 0x0080,
    BSP_GAMEPAD_LB = 0x0100, BSP_GAMEPAD_RB = 0x0200,
    BSP_GAMEPAD_GUIDE = 0x0400, BSP_GAMEPAD_A = 0x1000,
    BSP_GAMEPAD_B = 0x2000, BSP_GAMEPAD_X = 0x4000,
    BSP_GAMEPAD_Y = 0x8000
};

typedef struct {
    uint8_t connected; /* 由原始数据中的 USB 类状态复制 */
    uint8_t valid;     /* 报文通过格式检查时为 1 */
    uint16_t buttons;
    uint8_t lt, rt;
    int16_t lx, ly, rx, ry;
    /* 参考格式中的实验性运动字段，不代表已校准传感器数据。 */
    int16_t motion_h, motion_v;
    uint8_t motion_available;
} bsp_gamepad_data_t;

/* 获取并解包最新原始数据；格式无效或未连接时 valid 为 0，在任务上下文调用。 */
bsp_gamepad_data_t bsp_gamepad_get_data(void);

#endif
