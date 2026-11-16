#include "gamepad.h"
#include "gamepad_internal.h"
#include "gamepad_usb.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

/* USB Host 任务写入，应用任务读取；临界区保证跨任务复制的一致性。 */
static gamepad_raw_data_t latest;

uint8_t gamepad_send_data(const uint8_t *data, uint8_t length)
{
    return gamepad_usb_queue_send(data, length);
}

gamepad_tx_status_t gamepad_get_send_status(void)
{
    return gamepad_usb_send_status();
}

/**
 * 复制最近一次的原始数据快照，供任务读取。
 * @param snapshot 输出指针；为 NULL 时不执行任何操作。
 * @note 临界区保证复制时 USB Host 任务不会更新数据；仅在任务上下文调用。
 */
void gamepad_get_raw_data(gamepad_raw_data_t *snapshot)
{
    if (snapshot == NULL) return;
    taskENTER_CRITICAL();
    *snapshot = latest;
    taskEXIT_CRITICAL();
}

/**
 * 清空原始数据、连接状态和计数，在手柄类初始化时调用。
 * @note 由 USB Host 任务调用，临界区保护共享数据。
 */
void gamepad_store_reset(void)
{
    taskENTER_CRITICAL();
    memset(&latest, 0, sizeof(latest));
    taskEXIT_CRITICAL();
}

/**
 * 更新 USB 手柄类的连接状态。
 * @param connected 0 表示断开，非 0 表示手柄类已就绪。
 * @note 断开时清空报文长度和内容，保留当前计数；由 USB Host 任务调用。
 */
void gamepad_store_connected(uint8_t connected)
{
    taskENTER_CRITICAL();
    latest.connected = connected;
    if (!connected) { /* 断开时清除旧报文，避免应用层继续使用旧输入。 */
        latest.length = 0U;
        memset(latest.data, 0, sizeof(latest.data));
    }
    taskEXIT_CRITICAL();
}

/**
 * 保存一包 USB 原始报文，并增加报文计数。
 * @param data 指向本次接收数据的缓冲区；不能为 NULL。
 * @param length 本次实际接收长度，范围为 1 到 BSP_GAMEPAD_REPORT_MAX。
 * @note 参数无效时不更新数据；由 USB Host 任务调用。
 */
void gamepad_store_report(const uint8_t *data, uint8_t length)
{
    if (data == NULL || length == 0U || length > BSP_GAMEPAD_REPORT_MAX) return;
    taskENTER_CRITICAL();
    memcpy(latest.data, data, length);
    latest.length = length;
    latest.reports++;
    taskEXIT_CRITICAL();
}

/**
 * 增加当前连接的 USB 传输错误计数。
 * @note 由 USB Host 任务调用；此函数只记录错误，不负责恢复连接。
 */
void gamepad_store_error(void)
{
    taskENTER_CRITICAL();
    latest.errors++;
    taskEXIT_CRITICAL();
}
