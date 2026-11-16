#ifndef GAMEPAD_USB_H
#define GAMEPAD_USB_H

#include "usbh_core.h"
#include "gamepad.h"

/* 供 CubeMX 生成的 USB Host 初始化代码注册自定义手柄类。 */
extern USBH_ClassTypeDef USBH_Gamepad_Class;

/* 内部接口：只排队和查询，USB 操作由 Host 任务执行。 */
uint8_t gamepad_usb_queue_send(const uint8_t *data, uint8_t length);
gamepad_tx_status_t gamepad_usb_send_status(void);

#endif
