#ifndef GAMEPAD_APP_H
#define GAMEPAD_APP_H

#include "stm32f4xx_hal.h"

void Gamepad_AppInit(UART_HandleTypeDef *uart);
void Gamepad_AppProcess(void);

#endif
