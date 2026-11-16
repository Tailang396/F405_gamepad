//
// Created by asus on 2026/11/4.
//

#include "app_sys.h"
#include "bsp_uart.h"
#include "cmsis_os2.h"


void uart_debug_callback(bsp_uart_e e, uint8_t *s, uint16_t l);

void sys_main_init(void) {
    bsp_uart_init(E_UART_DEBUG, &huart1);
    bsp_uart_set_callback(E_UART_DEBUG, uart_debug_callback);
}

void app_sys(void *argument) {
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
    osDelay(2000);
    for(;;) {
        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
        osDelay(100);
    }
}


void uart_debug_callback(bsp_uart_e e, uint8_t *s, uint16_t l) {
    bsp_uart_send(e, s, l);
}



