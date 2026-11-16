#include "direwolf2.h"
#include <stdio.h>

/* Desktop demo timing only; on STM32 use the board's scheduler or timer. */
void demo_delay_ms(unsigned milliseconds);

/* Replace this array with a complete report received from the USB Host. */
int main(void) {
    const uint8_t report[] = {
        0x00, 0x14, 0x01, 0x10, 0x80, 0xff,
        0x00, 0x80, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    puts("Repeated simulated report (not live controller data). Stop with Ctrl+C.");
    for (;;) {
        dw2_state state;
        if (!dw2_parse_report(report, sizeof(report), &state)) {
            puts("Invalid or unsupported report");
            return 1;
        }
        printf("SIMULATED buttons=0x%04X A=%d B=%d X=%d Y=%d UP=%d "
               "LT=%u RT=%u LX=%d LY=%d RX=%d RY=%d",
               (unsigned)state.buttons, !!(state.buttons & DW2_A), !!(state.buttons & DW2_B),
               !!(state.buttons & DW2_X), !!(state.buttons & DW2_Y), !!(state.buttons & DW2_UP),
               (unsigned)state.lt, (unsigned)state.rt, (int)state.lx, (int)state.ly,
               (int)state.rx, (int)state.ry);
        if (state.motion_available)
            printf(" motion_h=%d motion_v=%d (experimental)", (int)state.motion_h, (int)state.motion_v);
        putchar('\n');
        fflush(stdout);
        demo_delay_ms(250);
    }
}
