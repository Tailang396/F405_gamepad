#ifndef GAMEPAD_RAW_DATA_H
#define GAMEPAD_RAW_DATA_H

#include <stdint.h>

#define BSP_GAMEPAD_REPORT_MAX 64U

/* USB 原始报文及其连接状态；不包含按键解析结果。 */
typedef struct {
    uint8_t connected; /* USB 手柄类是否就绪 */
    uint8_t length;    /* 最近一包的实际长度；断开时为 0 */
    uint8_t data[BSP_GAMEPAD_REPORT_MAX]; /* 最近一包原始字节 */
    uint32_t reports;  /* 当前连接已接收的非空报文数 */
    uint32_t errors;   /* 当前连接记录的 USB 传输错误数 */
} gamepad_raw_data_t;

/* 在任务上下文中，将完整原始数据复制到调用者的变量。 */
void gamepad_get_raw_data(gamepad_raw_data_t *out);

#endif
