#include "gamepad_app.h"
#include "usbh_direwolf.h"
#include "direwolf2.h"
#include <stdio.h>
#include <string.h>

static UART_HandleTypeDef *output_uart;
static uint8_t latest[64];
static uint32_t latest_length;
static uint32_t last_print;
static uint8_t report_pending;

static void write_uart(const char *text) {
    if (output_uart != NULL)
        (void)HAL_UART_Transmit(output_uart, (const uint8_t *)text, (uint16_t)strlen(text), 100U);
}

void Gamepad_AppInit(UART_HandleTypeDef *uart) {
    output_uart = uart;
    write_uart("\r\nDirewolf USB Host started; waiting for input.\r\n");
}

/* Called by the USB Host class in the main loop. Keep this fast. */
void Direwolf_OnReport(const uint8_t *data, uint32_t length) {
    if (length == 0U || length > sizeof latest) return;
    memcpy(latest, data, length);
    latest_length = length;
    report_pending = 1U;
}

void Gamepad_AppProcess(void) {
    uint32_t i;
    char line[256];
    size_t used;
    dw2_state state;
    if (!report_pending || (uint32_t)(HAL_GetTick() - last_print) < 200U) return;
    last_print = HAL_GetTick();
    report_pending = 0U;

    used = (size_t)snprintf(line, sizeof line, "RAW(%lu):", (unsigned long)latest_length);
    for (i = 0; i < latest_length && used + 4U < sizeof line; ++i)
        used += (size_t)snprintf(line + used, sizeof line - used, " %02X", (unsigned)latest[i]);
    if (used + 3U <= sizeof line) { line[used++] = '\r'; line[used++] = '\n'; line[used] = '\0'; }
    write_uart(line);

    if (!dw2_parse_report(latest, latest_length, &state)) {
        write_uart("Unknown report; raw data shown above.\r\n");
        return;
    }
    (void)snprintf(line, sizeof line,
                   "buttons=%04X A=%d B=%d X=%d Y=%d LT=%u RT=%u LX=%d LY=%d RX=%d RY=%d\r\n",
                   (unsigned)state.buttons, !!(state.buttons & DW2_A), !!(state.buttons & DW2_B),
                   !!(state.buttons & DW2_X), !!(state.buttons & DW2_Y),
                   (unsigned)state.lt, (unsigned)state.rt, (int)state.lx, (int)state.ly,
                   (int)state.rx, (int)state.ry);
    write_uart(line);
}
