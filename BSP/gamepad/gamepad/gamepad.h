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

typedef enum {
    GAMEPAD_TX_DISCONNECTED, /* OUT 端点不可用或手柄断开 */
    GAMEPAD_TX_IDLE,         /* 尚无发送请求 */
    GAMEPAD_TX_PENDING,      /* 已接受请求，等待 USB 发送完成 */
    GAMEPAD_TX_SENT,         /* USB 传输完成，不代表手柄一定执行了命令 */
    GAMEPAD_TX_FAILED        /* USB 传输失败 */
} gamepad_tx_status_t;

/* 在任务上下文提交一包原始数据，成功接受返回 1；忙、未连接或参数无效返回 0。
 * 内部复制数据，允许调用后复用 data；实际传输由 USB Host 任务处理。 */
uint8_t gamepad_send_data(const uint8_t *data, uint8_t length);

/* 查询最近一次发送状态；断开连接会取消待发送请求。 */
gamepad_tx_status_t gamepad_get_send_status(void);

#endif
