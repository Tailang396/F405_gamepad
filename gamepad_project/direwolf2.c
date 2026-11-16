#include "direwolf2.h"

static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static int16_t read_i16_le(const uint8_t *p) {
    uint16_t value = read_u16_le(p);
    /* Convert without relying on implementation-defined unsigned-to-signed casts. */
    return value < 0x8000u ? (int16_t)value : (int16_t)((int32_t)value - 65536);
}

static int16_t signed12(uint16_t value) {
    return value < 0x800u ? (int16_t)value : (int16_t)((int32_t)value - 4096);
}

int dw2_parse_report(const uint8_t *report, size_t length, dw2_state *out) {
    dw2_state state = {0};
    if (report == NULL || out == NULL || length < 14 ||
        report[0] != 0x00 || report[1] < 0x14) {
        return 0;
    }
    state.buttons = read_u16_le(report + 2);
    state.lt = report[4];
    state.rt = report[5];
    state.lx = read_i16_le(report + 6);
    state.ly = read_i16_le(report + 8);
    state.rx = read_i16_le(report + 10);
    state.ry = read_i16_le(report + 12);
    if (length >= 17) {
        state.motion_h = signed12((uint16_t)(report[14] | ((uint16_t)(report[16] >> 4) << 8)));
        state.motion_v = signed12((uint16_t)(report[15] | ((uint16_t)(report[16] & 0x0f) << 8)));
        state.motion_available = 1;
    }
    *out = state;
    return 1;
}
