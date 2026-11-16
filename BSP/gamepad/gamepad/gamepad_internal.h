#ifndef GAMEPAD_INTERNAL_H
#define GAMEPAD_INTERNAL_H

#include <stdint.h>

/* BSP 内部接口：由 USB Host 任务更新数据，应用层应使用 bsp_gamepad.h。 */
void gamepad_store_reset(void);
void gamepad_store_connected(uint8_t connected);
void gamepad_store_report(const uint8_t *data, uint8_t length);
void gamepad_store_error(void);

#endif
