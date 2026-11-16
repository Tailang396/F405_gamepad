#include "app_gamepad.h"
#include "bsp_gamepad.h"
#include "bsp_uart.h"
#include "cmsis_os2.h"

void app_gamepad(void *argument)
{
    bsp_gamepad_data_t gamepad;
    for (;;) {
        gamepad = bsp_gamepad_get_data();
        bsp_uart_printf(E_UART_DEBUG,
                        "gamepad connected=%u valid=%u buttons=0x%04X "
                        "LT=%u RT=%u LX=%d LY=%d RX=%d RY=%d "
                        "motion_available=%u motion_h=%d motion_v=%d\r\n",
                        (unsigned int)gamepad.connected, (unsigned int)gamepad.valid,
                        (unsigned int)gamepad.buttons, (unsigned int)gamepad.lt,
                        (unsigned int)gamepad.rt, (int)gamepad.lx, (int)gamepad.ly,
                        (int)gamepad.rx, (int)gamepad.ry,
                        (unsigned int)gamepad.motion_available,
                        (int)gamepad.motion_h, (int)gamepad.motion_v);
        osDelay(10U);
    }
}
