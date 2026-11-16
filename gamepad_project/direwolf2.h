#ifndef DIREWOLF2_H
#define DIREWOLF2_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Button bits in the USB Xbox-compatible input report. */
enum {
    DW2_UP = 0x0001, DW2_DOWN = 0x0002, DW2_LEFT = 0x0004,
    DW2_RIGHT = 0x0008, DW2_START = 0x0010, DW2_BACK = 0x0020,
    DW2_L3 = 0x0040, DW2_R3 = 0x0080, DW2_LB = 0x0100,
    DW2_RB = 0x0200, DW2_GUIDE = 0x0400, DW2_A = 0x1000,
    DW2_B = 0x2000, DW2_X = 0x4000, DW2_Y = 0x8000
};

typedef struct {
    uint16_t buttons;
    uint8_t lt, rt;
    int16_t lx, ly, rx, ry;
    /* Experimental: processed motion values, not calibrated gyro/acceleration. */
    int16_t motion_h, motion_v;
    uint8_t motion_available;
} dw2_state;

/* Parse one complete USB interrupt-IN payload, beginning with byte 0x00.
 * Returns 1 on success. On failure, *out is unchanged. No allocation or I/O.
 * This is not a generic HID report parser and does not drive a USB controller.
 */
int dw2_parse_report(const uint8_t *report, size_t length, dw2_state *out);

#ifdef __cplusplus
}
#endif
#endif
